# Smästtrafik

Smästtrafik tracks delays in Västtrafik bus traffic and ranks the lines, routes, and stops with the most delay over time.

## Stack

- C++20 backend with Crow
- PostgreSQL, with TimescaleDB support for `delay_observations`
- React, TypeScript, Vite, TanStack Query, TanStack Table, and Apache ECharts
- Docker Compose for API, worker, TimescaleDB/PostgreSQL, and Nginx

## Local Backend

The default native build uses the in-memory demo repository so it can run without PostgreSQL or Västtrafik credentials.

```bash
cmake -S . -B build/dev -DSMASTTRAFIK_BUILD_TESTS=ON
cmake --build build/dev --parallel
ctest --test-dir build/dev --output-on-failure
SMASTTRAFIK_PORT=8080 ./build/dev/smasttrafik-api
```

## Local Frontend

```bash
cd frontend
npm install
npm run dev
```

Vite proxies `/api/*` to `http://localhost:8080`.

## PostgreSQL Mode

Install `libpqxx` and configure the build with PostgreSQL enabled:

```bash
cmake -S . -B build/postgres -DSMASTTRAFIK_ENABLE_POSTGRES=ON
cmake --build build/postgres --parallel
DATABASE_URL=postgresql://smasttrafik:smasttrafik@localhost:5432/smasttrafik \
SMASTTRAFIK_USE_DEMO_REPOSITORY=false \
./build/postgres/smasttrafik-api
```

Run `smasttrafik-worker` with `VASTTRAFIK_CLIENT_ID` and `VASTTRAFIK_CLIENT_SECRET` to collect live data.

## Docker Compose

```bash
cp .env.example .env
docker compose up --build
```

The web UI is exposed at `http://localhost:8088`.

## Data Notes

Västtrafik credentials stay server-side. Never expose `VASTTRAFIK_CLIENT_ID` or `VASTTRAFIK_CLIENT_SECRET` in frontend builds, browser-readable config, or public logs.

Smästtrafik is an independent analysis tool, not an official Västtrafik service. It must not use the Västtrafik logo or official assets without permission, and it must not offer ticket or period-card purchases.

The worker defaults are compliance-first: departure details are disabled, polling runs every 600 seconds, upstream requests are limited to 20 per minute with a low burst, and raw upstream payload storage is off unless `SMASTTRAFIK_STORE_RAW_PAYLOADS=true`.

Public pages should attribute data as “Data från Västtrafiks öppna API:er. Smästtrafik är en fristående tjänst.” Docker Compose credentials in `.env.example` are local development samples only.
