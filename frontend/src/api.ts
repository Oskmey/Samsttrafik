export type Entity = 'line' | 'route' | 'stop';
export type Period = 'today' | 'week' | 'month' | 'year' | 'custom';
export type SortField = 'total_delay_minutes' | 'avg_delay_minutes' | 'delayed_departures' | 'route';
export type SortDirection = 'asc' | 'desc';
export type Bucket = 'hour' | 'day' | 'week';
export type Metric = 'avg_delay_minutes' | 'total_delay_minutes' | 'delayed_departures';

export interface Coverage {
  monitoredStopAreas: number;
  lines: number;
  stops: number;
  observedDepartures: number;
  earliestObservation: string;
  latestObservation: string;
  mode: string;
}

export interface Line {
  gid: string;
  designation: string;
  shortName: string;
  name: string;
  transportMode: string;
  backgroundColor: string;
  foregroundColor: string;
  borderColor: string;
}

export interface StopArea {
  gid: string;
  name: string;
  lat: number;
  lon: number;
}

export interface RankingRow {
  id: string;
  label: string;
  route: string;
  lineDesignation: string;
  stopName: string;
  totalDelayMinutes: number;
  avgDelayMinutes: number;
  delayedDepartures: number;
  observedDepartures: number;
}

export interface RankingPage {
  results: RankingRow[];
  page: number;
  pageSize: number;
  total: number;
  from: string;
  to: string;
}

export interface TrendPoint {
  bucketStart: string;
  value: number;
  delayedDepartures: number;
  observedDepartures: number;
}

export interface CompareSeries {
  id: string;
  label: string;
  points: TrendPoint[];
}

export interface Incident {
  situationNumber: string;
  severity: string;
  title: string;
  description: string;
  startTime: string;
  endTime: string;
}

async function request<T>(path: string, params?: Record<string, string | number | undefined>): Promise<T> {
  const url = new URL(path, window.location.origin);
  for (const [key, value] of Object.entries(params ?? {})) {
    if (value !== undefined && value !== '') {
      url.searchParams.set(key, String(value));
    }
  }

  const response = await fetch(url.pathname + url.search);
  if (!response.ok) {
    throw new Error(`Request failed with HTTP ${response.status}`);
  }
  return response.json() as Promise<T>;
}

export function getCoverage() {
  return request<Coverage>('/api/meta/coverage');
}

export function getLines(query = '') {
  return request<{ results: Line[] }>('/api/lines', { query, transportMode: 'bus' });
}

export function getStops(query = '', lineId = '') {
  return request<{ results: StopArea[] }>('/api/stops', { query, lineId });
}

export function getRankings(params: {
  entity: Entity;
  period: Period;
  from?: string;
  to?: string;
  sort: SortField;
  direction: SortDirection;
  page: number;
  pageSize: number;
  lineIds?: string;
  stopIds?: string;
}) {
  return request<RankingPage>('/api/delays/rankings', params);
}

export function getCompare(params: {
  entity: 'line' | 'stop';
  ids: string;
  from: string;
  to: string;
  bucket: Bucket;
  metric: Metric;
}) {
  return request<{ series: CompareSeries[] }>('/api/delays/compare', params);
}

export function getIncidents(params: { lineId?: string; stopId?: string; from: string; to: string }) {
  return request<{ results: Incident[] }>('/api/incidents', params);
}
