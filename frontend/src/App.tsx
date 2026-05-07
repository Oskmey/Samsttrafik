import { useQuery } from '@tanstack/react-query';
import {
  ColumnDef,
  flexRender,
  getCoreRowModel,
  useReactTable
} from '@tanstack/react-table';
import ReactECharts from 'echarts-for-react';
import {
  ArrowDown,
  ArrowUp,
  ArrowUpDown,
  CalendarDays,
  ChevronsLeftRight,
  RefreshCw,
  Search,
  TableProperties
} from 'lucide-react';
import { useMemo, useRef, useState } from 'react';

import {
  Bucket,
  Entity,
  Line,
  Metric,
  Period,
  RankingRow,
  SortDirection,
  SortField,
  getCompare,
  getCoverage,
  getIncidents,
  getLines,
  getRankings
} from './api';

const periods: Array<{ label: string; value: Period }> = [
  { label: 'Idag', value: 'today' },
  { label: 'Vecka', value: 'week' },
  { label: 'Månad', value: 'month' },
  { label: 'År', value: 'year' },
  { label: 'Eget', value: 'custom' }
];

const entityLabels: Array<{ label: string; value: Entity }> = [
  { label: 'Linjer', value: 'line' },
  { label: 'Rutter', value: 'route' },
  { label: 'Hållplatser', value: 'stop' }
];

const metricLabels: Array<{ label: string; value: Metric }> = [
  { label: 'Snitt', value: 'avg_delay_minutes' },
  { label: 'Totalt', value: 'total_delay_minutes' },
  { label: 'Avgångar', value: 'delayed_departures' }
];

function todayInput() {
  return new Date().toISOString().slice(0, 10);
}

function startOfWeekInput() {
  const date = new Date();
  date.setDate(date.getDate() - 6);
  return date.toISOString().slice(0, 10);
}

function stockholmOffset(value: string) {
  const [year, month, day] = value.split('-').map(Number);
  const probe = new Date(Date.UTC(year, month - 1, day, 12, 0, 0));
  const part = new Intl.DateTimeFormat('en-US', {
    timeZone: 'Europe/Stockholm',
    timeZoneName: 'shortOffset'
  })
    .formatToParts(probe)
    .find((item) => item.type === 'timeZoneName')?.value ?? 'GMT+1';
  const match = part.match(/GMT([+-])(\d{1,2})(?::(\d{2}))?/);
  if (!match) {
    return '+01:00';
  }
  return `${match[1]}${match[2].padStart(2, '0')}:${match[3] ?? '00'}`;
}

function toRfc3339(value: string, endOfDay = false) {
  if (!value) {
    return '';
  }
  return `${value}T${endOfDay ? '23:59:59' : '00:00:00'}${stockholmOffset(value)}`;
}

function fmt(value: number) {
  return new Intl.NumberFormat('sv-SE', { maximumFractionDigits: 1 }).format(value);
}

function shortDate(value: string) {
  if (!value) {
    return '';
  }
  return new Intl.DateTimeFormat('sv-SE', {
    month: 'short',
    day: 'numeric',
    hour: '2-digit'
  }).format(new Date(value));
}

function SortIcon({ field, sort, direction }: { field: SortField; sort: SortField; direction: SortDirection }) {
  if (field !== sort) {
    return <ArrowUpDown size={15} aria-hidden="true" />;
  }
  return direction === 'asc' ? <ArrowUp size={15} aria-hidden="true" /> : <ArrowDown size={15} aria-hidden="true" />;
}

function isExpressLine(designation: string) {
  return /^X\d*/i.test(designation.trim());
}

function lineBadgeClass(designation: string) {
  return `line-name-boxed ${isExpressLine(designation) ? 'line-name-boxed--express' : 'line-name-boxed--normal'}`;
}

function App() {
  const [entity, setEntity] = useState<Entity>('line');
  const [period, setPeriod] = useState<Period>('week');
  const [sort, setSort] = useState<SortField>('total_delay_minutes');
  const [direction, setDirection] = useState<SortDirection>('desc');
  const [page, setPage] = useState(1);
  const [fromDate, setFromDate] = useState(startOfWeekInput());
  const [toDate, setToDate] = useState(todayInput());
  const [lineSearch, setLineSearch] = useState('');
  const [selectedLines, setSelectedLines] = useState<string[]>([]);
  const [metric, setMetric] = useState<Metric>('avg_delay_minutes');
  const [bucket, setBucket] = useState<Bucket>('day');
  const lineSearchRef = useRef<HTMLInputElement>(null);

  const customFrom = period === 'custom' ? toRfc3339(fromDate) : '';
  const customTo = period === 'custom' ? toRfc3339(toDate, true) : '';

  const coverageQuery = useQuery({ queryKey: ['coverage'], queryFn: getCoverage });
  const linesQuery = useQuery({ queryKey: ['lines', lineSearch], queryFn: () => getLines(lineSearch) });
  const rankingsQuery = useQuery({
    queryKey: ['rankings', entity, period, customFrom, customTo, sort, direction, page],
    queryFn: () =>
      getRankings({
        entity,
        period,
        from: customFrom,
        to: customTo,
        sort,
        direction,
        page,
        pageSize: 25
      })
  });

  const activeRange = rankingsQuery.data;
  const compareIds = selectedLines.length > 0
    ? selectedLines.join(',')
    : rankingsQuery.data?.results.slice(0, 3).map((row) => row.id).join(',') ?? '';

  const compareQuery = useQuery({
    queryKey: ['compare', compareIds, activeRange?.from, activeRange?.to, bucket, metric],
    queryFn: () =>
      getCompare({
        entity: 'line',
        ids: compareIds,
        from: activeRange?.from ?? '',
        to: activeRange?.to ?? '',
        bucket,
        metric
      }),
    enabled: Boolean(compareIds && activeRange?.from && activeRange?.to)
  });

  const incidentsQuery = useQuery({
    queryKey: ['incidents', activeRange?.from, activeRange?.to],
    queryFn: () => getIncidents({ from: activeRange?.from ?? '', to: activeRange?.to ?? '' }),
    enabled: Boolean(activeRange?.from && activeRange?.to)
  });

  const columns = useMemo<ColumnDef<RankingRow>[]>(
    () => [
      {
        accessorKey: 'label',
        header: 'Objekt',
        cell: ({ row }) => (
          <div className="rank-title">
            <span className={row.original.lineDesignation ? lineBadgeClass(row.original.lineDesignation) : 'stop-label'}>
              {row.original.label}
            </span>
            <small>{row.original.route || row.original.stopName}</small>
          </div>
        )
      },
      {
        accessorKey: 'totalDelayMinutes',
        header: 'Totala minuter',
        cell: ({ row }) => fmt(row.original.totalDelayMinutes)
      },
      {
        accessorKey: 'avgDelayMinutes',
        header: 'Snitt',
        cell: ({ row }) => fmt(row.original.avgDelayMinutes)
      },
      {
        accessorKey: 'delayedDepartures',
        header: 'Försenade',
        cell: ({ row }) => row.original.delayedDepartures
      },
      {
        accessorKey: 'observedDepartures',
        header: 'Observerade',
        cell: ({ row }) => row.original.observedDepartures
      }
    ],
    []
  );

  const table = useReactTable({
    data: rankingsQuery.data?.results ?? [],
    columns,
    getCoreRowModel: getCoreRowModel(),
    manualSorting: true,
    manualPagination: true
  });

  const chartOption = useMemo(() => {
    const series = compareQuery.data?.series ?? [];
    const buckets = Array.from(new Set(series.flatMap((item) => item.points.map((point) => point.bucketStart)))).sort();
    return {
      color: ['#0079b4', '#009ddb', '#e76b18', '#d0021b', '#2f7d32', '#8060a5'],
      tooltip: { trigger: 'axis' },
      grid: { top: 24, right: 18, bottom: 34, left: 42 },
      xAxis: {
        type: 'category',
        data: buckets.map(shortDate),
        axisLine: { lineStyle: { color: '#dfe0e3' } },
        axisLabel: { color: '#3c4650' }
      },
      yAxis: {
        type: 'value',
        axisLine: { show: false },
        splitLine: { lineStyle: { color: '#dfe0e3' } },
        axisLabel: { color: '#3c4650' }
      },
      series: series.map((item) => ({
        name: item.label,
        type: 'line',
        smooth: true,
        symbolSize: 7,
        data: buckets.map((bucketStart) => item.points.find((point) => point.bucketStart === bucketStart)?.value ?? null)
      }))
    };
  }, [compareQuery.data]);

  function updateSort(field: SortField) {
    setPage(1);
    if (sort === field) {
      setDirection(direction === 'asc' ? 'desc' : 'asc');
      return;
    }
    setSort(field);
    setDirection(field === 'route' ? 'asc' : 'desc');
  }

  function toggleLine(line: Line) {
    setSelectedLines((current) => {
      if (current.includes(line.gid)) {
        return current.filter((id) => id !== line.gid);
      }
      return [...current, line.gid].slice(-5);
    });
  }

  const totalPages = Math.max(1, Math.ceil((rankingsQuery.data?.total ?? 0) / 25));

  return (
    <>
      <header className="site-header">
        <div className="site-header__inner">
          <a className="brand" href="/" aria-label="Smästtrafik startsida">
            <span className="brand-symbol">S</span>
            <span>Smästtrafik</span>
          </a>
          <nav className="main-nav" aria-label="Huvudnavigering">
            <span className="active">Observerad statistik</span>
            <span>Linjer</span>
            <span>Hållplatser</span>
            <span>Störningar</span>
          </nav>
          <div className="header-actions">
            <button type="button" onClick={() => lineSearchRef.current?.focus()} aria-label="Sök">
              <Search size={18} aria-hidden="true" />
              <span>Sök</span>
            </button>
            <button type="button" onClick={() => rankingsQuery.refetch()} aria-label="Uppdatera">
              <RefreshCw size={18} aria-hidden="true" />
              <span>Uppdatera</span>
            </button>
          </div>
        </div>
      </header>

      <main className="app-shell">
        <section className="page-intro">
          <div>
            <span className="eyebrow">Observerad statistik</span>
            <h1>Smästtrafik</h1>
            <p>Observerade bussförseningar från Västtrafiks öppna API:er, rankade över tid.</p>
            <p className="independence-notice">Oberoende analysverktyg. Inte anslutet till eller godkänt av Västtrafik.</p>
          </div>
          <div className="coverage">
            <span>{coverageQuery.data?.mode ?? 'demo'}</span>
            <strong>{coverageQuery.data?.observedDepartures ?? 0}</strong>
            <small>observerade avgångar</small>
          </div>
        </section>

        <section className="control-band">
        <div className="segmented" aria-label="Period">
          {periods.map((item) => (
            <button
              key={item.value}
              className={period === item.value ? 'active' : ''}
              onClick={() => {
                setPeriod(item.value);
                setPage(1);
              }}
            >
              {item.label}
            </button>
          ))}
        </div>

        <label className="field">
          <CalendarDays size={17} aria-hidden="true" />
          <input type="date" value={fromDate} disabled={period !== 'custom'} onChange={(event) => setFromDate(event.target.value)} />
        </label>
        <label className="field">
          <CalendarDays size={17} aria-hidden="true" />
          <input type="date" value={toDate} disabled={period !== 'custom'} onChange={(event) => setToDate(event.target.value)} />
        </label>

        <button className="icon-button" onClick={() => rankingsQuery.refetch()} aria-label="Uppdatera">
          <RefreshCw size={18} aria-hidden="true" />
        </button>
        </section>

        <section className="dashboard-grid">
        <div className="panel table-panel">
          <div className="panel-head">
            <div>
              <h2>Rankning</h2>
              <p>{activeRange ? `${shortDate(activeRange.from)} - ${shortDate(activeRange.to)}` : 'Laddar intervall'}</p>
            </div>
            <div className="segmented compact" aria-label="Objekt">
              {entityLabels.map((item) => (
                <button
                  key={item.value}
                  className={entity === item.value ? 'active' : ''}
                  onClick={() => {
                    setEntity(item.value);
                    setPage(1);
                  }}
                >
                  {item.label}
                </button>
              ))}
            </div>
          </div>

          <div className="sort-row">
            <button onClick={() => updateSort('total_delay_minutes')}>
              <SortIcon field="total_delay_minutes" sort={sort} direction={direction} /> Totalt
            </button>
            <button onClick={() => updateSort('avg_delay_minutes')}>
              <SortIcon field="avg_delay_minutes" sort={sort} direction={direction} /> Snitt
            </button>
            <button onClick={() => updateSort('delayed_departures')}>
              <SortIcon field="delayed_departures" sort={sort} direction={direction} /> Avgångar
            </button>
            <button onClick={() => updateSort('route')}>
              <SortIcon field="route" sort={sort} direction={direction} /> Rutt
            </button>
          </div>

          <div className="table-wrap">
            <table>
              <thead>
                {table.getHeaderGroups().map((headerGroup) => (
                  <tr key={headerGroup.id}>
                    {headerGroup.headers.map((header) => (
                      <th key={header.id}>{flexRender(header.column.columnDef.header, header.getContext())}</th>
                    ))}
                  </tr>
                ))}
              </thead>
              <tbody>
                {rankingsQuery.isLoading ? (
                  <tr>
                    <td colSpan={5}>Laddar...</td>
                  </tr>
                ) : table.getRowModel().rows.length === 0 ? (
                  <tr>
                    <td colSpan={5}>Inga observationer för valt urval.</td>
                  </tr>
                ) : (
                  table.getRowModel().rows.map((row) => (
                    <tr key={row.id}>
                      {row.getVisibleCells().map((cell) => (
                        <td key={cell.id}>{flexRender(cell.column.columnDef.cell, cell.getContext())}</td>
                      ))}
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>

          <div className="pager">
            <button disabled={page === 1} onClick={() => setPage((value) => Math.max(1, value - 1))}>
              Föregående
            </button>
            <span>
              {page} / {totalPages}
            </span>
            <button disabled={page >= totalPages} onClick={() => setPage((value) => Math.min(totalPages, value + 1))}>
              Nästa
            </button>
          </div>
        </div>

        <aside className="side-stack">
          <div className="panel">
            <div className="panel-head inline">
              <h2>Jämför</h2>
              <ChevronsLeftRight size={18} aria-hidden="true" />
            </div>
            <label className="field full">
              <Search size={17} aria-hidden="true" />
              <input ref={lineSearchRef} value={lineSearch} onChange={(event) => setLineSearch(event.target.value)} placeholder="Sök linje" />
            </label>
            <div className="line-picker">
              {(linesQuery.data?.results ?? []).slice(0, 8).map((line) => (
                <button
                  key={line.gid}
                  className={selectedLines.includes(line.gid) ? 'selected' : ''}
                  onClick={() => toggleLine(line)}
                >
                  <span className={lineBadgeClass(line.designation)}>{line.designation}</span>
                  <small>{line.name}</small>
                </button>
              ))}
            </div>
          </div>

          <div className="panel chart-panel">
            <div className="panel-head">
              <div>
                <h2>Trend</h2>
                <p>{compareQuery.data?.series.length ?? 0} serier</p>
              </div>
              <div className="chart-tools">
                <select value={metric} onChange={(event) => setMetric(event.target.value as Metric)}>
                  {metricLabels.map((item) => (
                    <option key={item.value} value={item.value}>
                      {item.label}
                    </option>
                  ))}
                </select>
                <select value={bucket} onChange={(event) => setBucket(event.target.value as Bucket)}>
                  <option value="hour">Timme</option>
                  <option value="day">Dag</option>
                  <option value="week">Vecka</option>
                </select>
              </div>
            </div>
            <ReactECharts option={chartOption} style={{ height: 260 }} notMerge lazyUpdate />
          </div>

          <div className="panel incidents">
            <div className="panel-head inline">
              <h2>Störningar</h2>
              <TableProperties size={18} aria-hidden="true" />
            </div>
            {(incidentsQuery.data?.results ?? []).slice(0, 3).map((incident) => (
              <article key={incident.situationNumber}>
                <strong>{incident.title}</strong>
                <span>{incident.severity || 'okänd'}</span>
              </article>
            ))}
            {(incidentsQuery.data?.results ?? []).length === 0 && <p className="muted">Inga aktiva störningar i valt intervall.</p>}
          </div>
        </aside>
        </section>

      </main>

      <footer className="site-footer">
        <div>
          <strong>Smästtrafik</strong>
          <span>Data från Västtrafiks öppna API:er. Smästtrafik är en fristående tjänst.</span>
        </div>
      </footer>
    </>
  );
}

export default App;
