create table if not exists lines (
    gid text primary key,
    designation text not null default '',
    short_name text not null default '',
    name text not null default '',
    transport_mode text not null default 'bus',
    background_color text,
    foreground_color text,
    border_color text,
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now()
);

create table if not exists stop_areas (
    gid text primary key,
    name text not null,
    lat double precision,
    lon double precision,
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now()
);

create table if not exists stop_points (
    gid text primary key,
    stop_area_gid text references stop_areas(gid),
    name text not null default '',
    platform text,
    lat double precision,
    lon double precision,
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now()
);

create table if not exists service_journeys (
    gid text primary key,
    traffic_day date not null,
    line_gid text not null references lines(gid),
    direction text,
    first_seen_at timestamptz not null default now(),
    last_seen_at timestamptz not null default now()
);

create table if not exists departure_calls (
    id bigserial primary key,
    service_journey_gid text not null references service_journeys(gid),
    stop_point_gid text not null references stop_points(gid),
    planned_departure_at timestamptz not null,
    estimated_departure_at timestamptz,
    delay_seconds integer not null default 0,
    is_cancelled boolean not null default false,
    is_part_cancelled boolean not null default false,
    realtime_stop_point_gid text,
    source text not null default 'vasttrafik-planera-resa-v4',
    details_reference text,
    raw jsonb not null default '{}'::jsonb,
    first_seen_at timestamptz not null default now(),
    last_seen_at timestamptz not null default now(),
    unique (service_journey_gid, stop_point_gid, planned_departure_at)
);

create table if not exists delay_observations (
    id bigserial,
    departure_call_id bigint not null references departure_calls(id) on delete cascade,
    observed_at timestamptz not null default now(),
    observed_estimated_departure_at timestamptz,
    delay_seconds integer not null default 0,
    is_cancelled boolean not null default false,
    raw_hash text,
    raw jsonb not null default '{}'::jsonb,
    primary key (id, observed_at)
);

do $$
begin
    create extension if not exists timescaledb;
exception
    when undefined_file then
        raise notice 'TimescaleDB extension is not installed; continuing with a normal PostgreSQL table.';
end $$;

do $$
begin
    if exists (select 1 from pg_proc where proname = 'create_hypertable') then
        perform create_hypertable('delay_observations', 'observed_at', if_not_exists => true);
    end if;
end $$;

create table if not exists traffic_situations (
    situation_number text primary key,
    severity text,
    title text,
    description text,
    start_time timestamptz,
    end_time timestamptz,
    affected_line_gids jsonb not null default '[]'::jsonb,
    affected_stop_area_gids jsonb not null default '[]'::jsonb,
    raw jsonb not null default '{}'::jsonb,
    created_at timestamptz not null default now(),
    updated_at timestamptz not null default now()
);

create table if not exists fetch_runs (
    id bigserial primary key,
    started_at timestamptz not null default now(),
    endpoint text not null,
    status_code integer not null default 0,
    latency_ms integer not null default 0,
    rows_fetched integer not null default 0,
    error text,
    rate_limit_remaining integer,
    backoff_until timestamptz
);

create index if not exists idx_lines_transport_mode on lines (transport_mode);
create index if not exists idx_stop_points_area on stop_points (stop_area_gid);
create index if not exists idx_service_journeys_line_day on service_journeys (line_gid, traffic_day);
create index if not exists idx_departure_calls_planned on departure_calls (planned_departure_at);
create index if not exists idx_departure_calls_delay on departure_calls (delay_seconds);
create index if not exists idx_departure_calls_stop on departure_calls (stop_point_gid, planned_departure_at);
create index if not exists idx_delay_observations_departure on delay_observations (departure_call_id, observed_at desc);
create index if not exists idx_traffic_situations_time on traffic_situations (start_time, end_time);
