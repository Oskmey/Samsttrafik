#include "smasttrafik/repository.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <pqxx/pqxx>

#include "smasttrafik/time_utils.hpp"

namespace smasttrafik {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("could not read migration " + path.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string quote_list(pqxx::transaction_base& tx, const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) {
            result += ",";
        }
        result += tx.quote(value);
    }
    return result;
}

std::string sort_expression(const std::string& sort) {
    if (sort == "avg_delay_minutes") {
        return "avg_delay_minutes";
    }
    if (sort == "delayed_departures") {
        return "delayed_departures";
    }
    if (sort == "route") {
        return "route";
    }
    return "total_delay_minutes";
}

std::string entity_select(const std::string& entity) {
    if (entity == "stop") {
        return R"SQL(
            coalesce(sa.gid, sp.stop_area_gid) as id,
            coalesce(sa.name, sp.name) as label,
            coalesce(sa.name, sp.name) as route,
            '' as line_designation,
            coalesce(sa.name, sp.name) as stop_name
        )SQL";
    }
    if (entity == "route") {
        return R"SQL(
            sj.line_gid || ':' || coalesce(nullif(sj.direction, ''), 'unknown') as id,
            l.designation || ' ' || coalesce(nullif(sj.direction, ''), 'Okand riktning') as label,
            l.designation || ' ' || coalesce(nullif(sj.direction, ''), 'Okand riktning') as route,
            l.designation as line_designation,
            '' as stop_name
        )SQL";
    }
    return R"SQL(
        l.gid as id,
        l.designation as label,
        l.designation || ' ' || coalesce(nullif(l.name, ''), '') as route,
        l.designation as line_designation,
        '' as stop_name
    )SQL";
}

std::string bucket_sql(const std::string& bucket) {
    if (bucket == "hour") {
        return "hour";
    }
    if (bucket == "week") {
        return "week";
    }
    return "day";
}

std::vector<Json> situation_items(const Json& payload) {
    if (payload.is_array()) {
        return payload.get<std::vector<Json>>();
    }
    if (payload.contains("results") && payload["results"].is_array()) {
        return payload["results"].get<std::vector<Json>>();
    }
    if (payload.contains("trafficSituations") && payload["trafficSituations"].is_array()) {
        return payload["trafficSituations"].get<std::vector<Json>>();
    }
    return {};
}

std::string json_string(const Json& value, const std::initializer_list<const char*> keys, const std::string& fallback = "") {
    const Json* current = &value;
    for (const char* key : keys) {
        if (!current->is_object() || !current->contains(key)) {
            return fallback;
        }
        current = &(*current)[key];
    }
    if (current->is_string()) {
        return current->get<std::string>();
    }
    if (current->is_number_integer()) {
        return std::to_string(current->get<long long>());
    }
    return fallback;
}

class PostgresRepository final : public Repository {
public:
    explicit PostgresRepository(Config config) : config_(std::move(config)) {
        if (config_.database_url.empty()) {
            throw std::runtime_error("DATABASE_URL or SMASTTRAFIK_DATABASE_URL is required for PostgreSQL mode");
        }
    }

    bool is_persistent() const override { return true; }
    std::string mode() const override { return "postgres"; }

    void migrate() override {
        pqxx::connection connection(config_.database_url);
        pqxx::work tx(connection);

        if (!std::filesystem::exists(config_.migrations_dir)) {
            throw std::runtime_error("migrations directory not found: " + config_.migrations_dir);
        }

        std::vector<std::filesystem::path> migrations;
        for (const auto& entry : std::filesystem::directory_iterator(config_.migrations_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".sql") {
                migrations.push_back(entry.path());
            }
        }
        std::sort(migrations.begin(), migrations.end());

        for (const auto& migration : migrations) {
            tx.exec(read_file(migration));
        }
        tx.commit();
    }

    Coverage coverage() override {
        pqxx::connection connection(config_.database_url);
        pqxx::read_transaction tx(connection);
        const auto row = tx.exec(R"SQL(
            select
                (select count(*) from stop_areas)::int as stops,
                (select count(*) from lines where transport_mode = 'bus')::int as lines,
                (select count(*) from departure_calls)::int as observed_departures,
                coalesce(to_char(min(observed_at), 'YYYY-MM-DD"T"HH24:MI:SSOF'), '') as earliest_observation,
                coalesce(to_char(max(observed_at), 'YYYY-MM-DD"T"HH24:MI:SSOF'), '') as latest_observation
            from delay_observations
        )SQL").one_row();

        Coverage coverage;
        coverage.monitored_stop_areas = static_cast<int>(config_.monitored_stop_areas.size());
        coverage.stops = row["stops"].as<int>(0);
        coverage.lines = row["lines"].as<int>(0);
        coverage.observed_departures = row["observed_departures"].as<int>(0);
        coverage.earliest_observation = row["earliest_observation"].as<std::string>("");
        coverage.latest_observation = row["latest_observation"].as<std::string>("");
        coverage.mode = mode();
        return coverage;
    }

    std::vector<Line> find_lines(const std::string& query, const std::string& transport_mode) override {
        pqxx::connection connection(config_.database_url);
        pqxx::read_transaction tx(connection);
        auto rows = tx.exec_params(R"SQL(
            select gid, designation, short_name, name, transport_mode, background_color, foreground_color, border_color
            from lines
            where ($1 = '' or designation ilike '%' || $1 || '%' or name ilike '%' || $1 || '%')
              and ($2 = '' or transport_mode = $2)
            order by designation
            limit 100
        )SQL", query, transport_mode);

        std::vector<Line> result;
        for (const auto& row : rows) {
            result.push_back({
                row["gid"].as<std::string>(),
                row["designation"].as<std::string>(""),
                row["short_name"].as<std::string>(""),
                row["name"].as<std::string>(""),
                row["transport_mode"].as<std::string>(""),
                row["background_color"].as<std::string>(""),
                row["foreground_color"].as<std::string>(""),
                row["border_color"].as<std::string>(""),
            });
        }
        return result;
    }

    std::vector<StopArea> find_stops(const std::string& query, const std::string& line_id) override {
        pqxx::connection connection(config_.database_url);
        pqxx::read_transaction tx(connection);
        std::string sql = R"SQL(
            select distinct sa.gid, sa.name, sa.lat, sa.lon
            from stop_areas sa
            left join stop_points sp on sp.stop_area_gid = sa.gid
            left join departure_calls dc on dc.stop_point_gid = sp.gid
            left join service_journeys sj on sj.gid = dc.service_journey_gid
            where ($1 = '' or sa.name ilike '%' || $1 || '%')
        )SQL";
        if (!line_id.empty()) {
            sql += " and sj.line_gid = " + tx.quote(line_id);
        }
        sql += " order by sa.name limit 100";

        auto rows = tx.exec_params(sql, query);
        std::vector<StopArea> result;
        for (const auto& row : rows) {
            result.push_back({
                row["gid"].as<std::string>(),
                row["name"].as<std::string>(""),
                row["lat"].as<double>(0.0),
                row["lon"].as<double>(0.0),
            });
        }
        return result;
    }

    RankingPage rankings(const RankingQuery& query) override {
        pqxx::connection connection(config_.database_url);
        pqxx::read_transaction tx(connection);

        std::string filters;
        if (!query.line_ids.empty()) {
            filters += " and sj.line_gid in (" + quote_list(tx, query.line_ids) + ")";
        }
        if (!query.stop_ids.empty()) {
            filters += " and coalesce(sa.gid, sp.stop_area_gid) in (" + quote_list(tx, query.stop_ids) + ")";
        }

        const std::string select = entity_select(query.entity);
        const std::string order = sort_expression(query.sort) + (query.direction == "asc" ? " asc" : " desc");
        const int offset = (query.page - 1) * query.page_size;

        std::ostringstream sql;
        sql << R"SQL(
            with grouped as (
                select
        )SQL" << select << R"SQL(,
                    sum(greatest(dc.delay_seconds, 0)) / 60.0 as total_delay_minutes,
                    count(*) filter (where dc.delay_seconds >= 60) as delayed_departures,
                    count(*) as observed_departures
                from departure_calls dc
                join service_journeys sj on sj.gid = dc.service_journey_gid
                join lines l on l.gid = sj.line_gid
                join stop_points sp on sp.gid = dc.stop_point_gid
                left join stop_areas sa on sa.gid = sp.stop_area_gid
                where dc.planned_departure_at >= $1::timestamptz
                  and dc.planned_departure_at < $2::timestamptz
        )SQL" << filters << R"SQL(
                group by id, label, route, line_designation, stop_name
            ), ranked as (
                select
                    *,
                    case when delayed_departures = 0 then 0
                         else total_delay_minutes / delayed_departures end as avg_delay_minutes,
                    count(*) over() as total_rows
                from grouped
            )
            select *
            from ranked
            order by )SQL" << order << R"SQL(, label asc
            limit $3 offset $4
        )SQL";

        auto rows = tx.exec_params(sql.str(), query.from, query.to, query.page_size, offset);
        RankingPage page;
        page.page = query.page;
        page.page_size = query.page_size;
        page.from = query.from;
        page.to = query.to;
        for (const auto& row : rows) {
            page.rows.push_back({
                row["id"].as<std::string>(),
                row["label"].as<std::string>(""),
                row["route"].as<std::string>(""),
                row["line_designation"].as<std::string>(""),
                row["stop_name"].as<std::string>(""),
                row["total_delay_minutes"].as<double>(0.0),
                row["avg_delay_minutes"].as<double>(0.0),
                row["delayed_departures"].as<int>(0),
                row["observed_departures"].as<int>(0),
            });
            page.total = row["total_rows"].as<int>(0);
        }
        return page;
    }

    std::vector<CompareSeries> compare(const CompareQuery& query) override {
        pqxx::connection connection(config_.database_url);
        pqxx::read_transaction tx(connection);

        if (query.ids.empty()) {
            return {};
        }

        const std::string id_column = query.entity == "stop" ? "coalesce(sa.gid, sp.stop_area_gid)" : "sj.line_gid";
        const std::string label_column = query.entity == "stop" ? "coalesce(sa.name, sp.name)" : "l.designation";
        const std::string metric = query.metric == "total_delay_minutes"
            ? "sum(greatest(dc.delay_seconds, 0)) / 60.0"
            : (query.metric == "delayed_departures"
                ? "count(*) filter (where dc.delay_seconds >= 60)::float"
                : "case when count(*) filter (where dc.delay_seconds >= 60) = 0 then 0 else (sum(greatest(dc.delay_seconds, 0)) / 60.0) / count(*) filter (where dc.delay_seconds >= 60) end");

        std::ostringstream sql;
        sql << R"SQL(
            select
                )SQL" << id_column << R"SQL( as id,
                )SQL" << label_column << R"SQL( as label,
                to_char(date_trunc(')SQL" << bucket_sql(query.bucket) << R"SQL(', dc.planned_departure_at), 'YYYY-MM-DD"T"HH24:MI:SSOF') as bucket_start,
                )SQL" << metric << R"SQL( as value,
                count(*) filter (where dc.delay_seconds >= 60)::int as delayed_departures,
                count(*)::int as observed_departures
            from departure_calls dc
            join service_journeys sj on sj.gid = dc.service_journey_gid
            join lines l on l.gid = sj.line_gid
            join stop_points sp on sp.gid = dc.stop_point_gid
            left join stop_areas sa on sa.gid = sp.stop_area_gid
            where dc.planned_departure_at >= $1::timestamptz
              and dc.planned_departure_at < $2::timestamptz
              and )SQL" << id_column << R"SQL( in ()SQL" << quote_list(tx, query.ids) << R"SQL()
            group by id, label, bucket_start
            order by bucket_start asc, label asc
        )SQL";

        auto rows = tx.exec_params(sql.str(), query.from, query.to);
        std::vector<CompareSeries> result;
        for (const auto& row : rows) {
            const std::string id = row["id"].as<std::string>();
            auto found = std::find_if(result.begin(), result.end(), [&](const CompareSeries& item) {
                return item.id == id;
            });
            if (found == result.end()) {
                result.push_back({id, row["label"].as<std::string>(""), {}});
                found = std::prev(result.end());
            }
            found->points.push_back({
                row["bucket_start"].as<std::string>(""),
                row["value"].as<double>(0.0),
                row["delayed_departures"].as<int>(0),
                row["observed_departures"].as<int>(0),
            });
        }
        return result;
    }

    std::vector<Incident> incidents(
        const std::string& line_id,
        const std::string& stop_id,
        const std::string& from,
        const std::string& to
    ) override {
        pqxx::connection connection(config_.database_url);
        pqxx::read_transaction tx(connection);

        std::string filters;
        if (!line_id.empty()) {
            filters += " and affected_line_gids @> " + tx.quote(Json::array({line_id}).dump()) + "::jsonb";
        }
        if (!stop_id.empty()) {
            filters += " and affected_stop_area_gids @> " + tx.quote(Json::array({stop_id}).dump()) + "::jsonb";
        }

        std::string sql = R"SQL(
            select situation_number, severity, title, description,
                   coalesce(to_char(start_time, 'YYYY-MM-DD"T"HH24:MI:SSOF'), '') as start_time,
                   coalesce(to_char(end_time, 'YYYY-MM-DD"T"HH24:MI:SSOF'), '') as end_time
            from traffic_situations
            where (end_time is null or end_time >= $1::timestamptz)
              and (start_time is null or start_time < $2::timestamptz)
        )SQL";
        sql += filters;
        sql += " order by start_time desc nulls last limit 100";
        const auto rows = tx.exec_params(sql, from, to);

        std::vector<Incident> result;
        for (const auto& row : rows) {
            result.push_back({
                row["situation_number"].as<std::string>(),
                row["severity"].as<std::string>(""),
                row["title"].as<std::string>(""),
                row["description"].as<std::string>(""),
                row["start_time"].as<std::string>(""),
                row["end_time"].as<std::string>(""),
            });
        }
        return result;
    }

    void record_fetch_run(const FetchRun& run) override {
        pqxx::connection connection(config_.database_url);
        pqxx::work tx(connection);
        tx.exec_params(R"SQL(
            insert into fetch_runs (endpoint, status_code, latency_ms, rows_fetched, error, rate_limit_remaining, backoff_until)
            values ($1, $2, $3, $4, nullif($5, ''), nullif($6::integer, -1), nullif($7, '')::timestamptz)
        )SQL", run.endpoint, run.status_code, run.latency_ms, run.rows_fetched, run.error, run.rate_limit_remaining, run.backoff_until);
        tx.commit();
    }

    std::string active_backoff_until() override {
        pqxx::connection connection(config_.database_url);
        pqxx::read_transaction tx(connection);
        const auto rows = tx.exec(R"SQL(
            select coalesce(to_char(backoff_until, 'YYYY-MM-DD"T"HH24:MI:SSOF'), '') as backoff_until
            from fetch_runs
            where backoff_until is not null
              and backoff_until > now()
            order by backoff_until desc
            limit 1
        )SQL");
        if (rows.empty()) {
            return {};
        }
        return rows[0]["backoff_until"].as<std::string>("");
    }

    void upsert_stop_areas(const std::vector<StopArea>& stops) override {
        pqxx::connection connection(config_.database_url);
        pqxx::work tx(connection);
        for (const auto& stop : stops) {
            tx.exec_params(R"SQL(
                insert into stop_areas (gid, name, lat, lon)
                values ($1, $2, $3, $4)
                on conflict (gid) do update set
                    name = excluded.name,
                    lat = excluded.lat,
                    lon = excluded.lon,
                    updated_at = now()
            )SQL", stop.gid, stop.name, stop.latitude, stop.longitude);
        }
        tx.commit();
    }

    void upsert_departures(const std::vector<CollectedDeparture>& departures) override {
        pqxx::connection connection(config_.database_url);
        pqxx::work tx(connection);
        for (const auto& departure : departures) {
            if (departure.service_journey.gid.empty() || departure.stop_point.gid.empty() || departure.planned_departure_at.empty()) {
                continue;
            }
            const Json stored_raw = config_.store_raw_payloads ? departure.raw : Json::object();
            const Json observation_fingerprint = {
                {"plannedDepartureAt", departure.planned_departure_at},
                {"estimatedDepartureAt", departure.estimated_departure_at},
                {"delaySeconds", departure.delay_seconds},
                {"isCancelled", departure.is_cancelled},
                {"isPartCancelled", departure.is_part_cancelled},
                {"realtimeStopPointGid", departure.realtime_stop_point.gid},
            };

            tx.exec_params(R"SQL(
                insert into lines (gid, designation, short_name, name, transport_mode, background_color, foreground_color, border_color)
                values ($1, $2, $3, $4, $5, nullif($6, ''), nullif($7, ''), nullif($8, ''))
                on conflict (gid) do update set
                    designation = excluded.designation,
                    short_name = excluded.short_name,
                    name = excluded.name,
                    transport_mode = excluded.transport_mode,
                    background_color = excluded.background_color,
                    foreground_color = excluded.foreground_color,
                    border_color = excluded.border_color,
                    updated_at = now()
            )SQL",
                departure.line.gid,
                departure.line.designation,
                departure.line.short_name,
                departure.line.name,
                departure.line.transport_mode.empty() ? "bus" : departure.line.transport_mode,
                departure.line.background_color,
                departure.line.foreground_color,
                departure.line.border_color);

            if (!departure.stop_point.stop_area_gid.empty()) {
                tx.exec_params(R"SQL(
                    insert into stop_areas (gid, name, lat, lon)
                    values ($1, $2, $3, $4)
                    on conflict (gid) do update set
                        name = coalesce(nullif(excluded.name, ''), stop_areas.name),
                        lat = excluded.lat,
                        lon = excluded.lon,
                        updated_at = now()
                )SQL",
                    departure.stop_point.stop_area_gid,
                    departure.stop_point.name,
                    departure.stop_point.latitude,
                    departure.stop_point.longitude);
            }

            tx.exec_params(R"SQL(
                insert into stop_points (gid, stop_area_gid, name, platform, lat, lon)
                values ($1, nullif($2, ''), $3, nullif($4, ''), $5, $6)
                on conflict (gid) do update set
                    stop_area_gid = coalesce(excluded.stop_area_gid, stop_points.stop_area_gid),
                    name = excluded.name,
                    platform = excluded.platform,
                    lat = excluded.lat,
                    lon = excluded.lon,
                    updated_at = now()
            )SQL",
                departure.stop_point.gid,
                departure.stop_point.stop_area_gid,
                departure.stop_point.name,
                departure.stop_point.platform,
                departure.stop_point.latitude,
                departure.stop_point.longitude);

            tx.exec_params(R"SQL(
                insert into service_journeys (gid, traffic_day, line_gid, direction, first_seen_at, last_seen_at)
                values ($1, $2, $3, nullif($4, ''), now(), now())
                on conflict (gid) do update set
                    traffic_day = excluded.traffic_day,
                    line_gid = excluded.line_gid,
                    direction = excluded.direction,
                    last_seen_at = now()
            )SQL",
                departure.service_journey.gid,
                departure.service_journey.traffic_day,
                departure.service_journey.line_gid.empty() ? departure.line.gid : departure.service_journey.line_gid,
                departure.service_journey.direction);

            const auto departure_row = tx.exec_params(R"SQL(
                insert into departure_calls (
                    service_journey_gid,
                    stop_point_gid,
                    planned_departure_at,
                    estimated_departure_at,
                    delay_seconds,
                    is_cancelled,
                    is_part_cancelled,
                    realtime_stop_point_gid,
                    source,
                    details_reference,
                    raw,
                    first_seen_at,
                    last_seen_at
                )
                values ($1, $2, $3::timestamptz, nullif($4, '')::timestamptz, $5, $6, $7, nullif($8, ''), 'vasttrafik-planera-resa-v4', nullif($9, ''), $10::jsonb, now(), now())
                on conflict (service_journey_gid, stop_point_gid, planned_departure_at) do update set
                    estimated_departure_at = excluded.estimated_departure_at,
                    delay_seconds = excluded.delay_seconds,
                    is_cancelled = excluded.is_cancelled,
                    is_part_cancelled = excluded.is_part_cancelled,
                    realtime_stop_point_gid = excluded.realtime_stop_point_gid,
                    details_reference = excluded.details_reference,
                    raw = excluded.raw,
                    last_seen_at = now()
                returning id
            )SQL",
                departure.service_journey.gid,
                departure.stop_point.gid,
                departure.planned_departure_at,
                departure.estimated_departure_at,
                departure.delay_seconds,
                departure.is_cancelled,
                departure.is_part_cancelled,
                departure.realtime_stop_point.gid,
                departure.details_reference,
                stored_raw.dump()).one_row();

            tx.exec_params(R"SQL(
                insert into delay_observations (
                    departure_call_id,
                    observed_estimated_departure_at,
                    delay_seconds,
                    is_cancelled,
                    raw_hash,
                    raw
                )
                select $1, nullif($2, '')::timestamptz, $3, $4, md5($5), $6::jsonb
                where not exists (
                    select 1
                    from (
                        select raw_hash
                        from delay_observations
                        where departure_call_id = $1
                        order by observed_at desc
                        limit 1
                    ) latest
                    where latest.raw_hash = md5($5)
                )
            )SQL",
                departure_row["id"].as<long long>(),
                departure.estimated_departure_at,
                departure.delay_seconds,
                departure.is_cancelled,
                observation_fingerprint.dump(),
                stored_raw.dump());
        }
        tx.commit();
    }

    void upsert_traffic_situations(const Json& situations) override {
        pqxx::connection connection(config_.database_url);
        pqxx::work tx(connection);
        for (const auto& situation : situation_items(situations)) {
            const std::string situation_number = json_string(situation, {"situationNumber"}, json_string(situation, {"id"}));
            if (situation_number.empty()) {
                continue;
            }
            const std::string severity = json_string(situation, {"severity"});
            const std::string title = json_string(situation, {"title"});
            const std::string description = json_string(situation, {"description"});
            const std::string start = json_string(situation, {"startTime"}, json_string(situation, {"start"}));
            const std::string end = json_string(situation, {"endTime"}, json_string(situation, {"end"}));
            const Json stored_raw = config_.store_raw_payloads ? situation : Json::object();

            tx.exec_params(R"SQL(
                insert into traffic_situations (
                    situation_number,
                    severity,
                    title,
                    description,
                    start_time,
                    end_time,
                    affected_line_gids,
                    affected_stop_area_gids,
                    raw,
                    updated_at
                )
                values ($1, $2, $3, $4, nullif($5, '')::timestamptz, nullif($6, '')::timestamptz, '[]'::jsonb, '[]'::jsonb, $7::jsonb, now())
                on conflict (situation_number) do update set
                    severity = excluded.severity,
                    title = excluded.title,
                    description = excluded.description,
                    start_time = excluded.start_time,
                    end_time = excluded.end_time,
                    raw = excluded.raw,
                    updated_at = now()
            )SQL", situation_number, severity, title, description, start, end, stored_raw.dump());
        }
        tx.commit();
    }

private:
    Config config_;
};

} // namespace

std::shared_ptr<Repository> make_repository(const Config& config) {
    if (config.use_demo_repository) {
        return make_demo_repository(config);
    }
    return std::make_shared<PostgresRepository>(config);
}

} // namespace smasttrafik
