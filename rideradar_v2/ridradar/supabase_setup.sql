-- ==============================================================================
-- RideRadar — Supabase Setup Script
-- Project URL: https://kruwarhbrxliiuiwcdcf.supabase.co
--
-- This script sets up:
-- 1. The `buses` table (primary table used by RideRadar backend & frontend)
-- 2. Row Level Security (RLS) ensuring public/anon can only READ (SELECT)
-- 3. Supabase Realtime publication for live GPS map streaming
-- 4. Initial starter telemetry row for device `tbeam_001` in Surat
-- 5. A compatibility view `bus_locations` for backward compatibility
-- ==============================================================================

-- 1) CREATE THE TABLE
CREATE TABLE IF NOT EXISTS public.buses (
    id TEXT PRIMARY KEY,                           -- Device ID (e.g. 'tbeam_001')
    bus_id TEXT DEFAULT 'BRTS_01',                 -- Bus / Route identifier
    device_id TEXT DEFAULT 'tbeam_001',            -- Hardware tracker identifier
    lat DOUBLE PRECISION NOT NULL,                 -- Latitude coordinate
    lng DOUBLE PRECISION NOT NULL,                 -- Longitude coordinate
    speed DOUBLE PRECISION DEFAULT 0.0,            -- Speed in km/h
    heading DOUBLE PRECISION DEFAULT 0.0,          -- Direction / bearing in degrees
    satellites INTEGER DEFAULT 0,                  -- Number of locked GPS satellites
    created_at TIMESTAMPTZ DEFAULT now() NOT NULL, -- Initial creation timestamp
    updated_at TIMESTAMPTZ DEFAULT now() NOT NULL  -- Last updated timestamp
);

-- Ensure full payload is sent during Realtime updates
ALTER TABLE public.buses REPLICA IDENTITY FULL;

-- Automatic updated_at trigger
CREATE OR REPLACE FUNCTION public.handle_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = now();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_set_updated_at ON public.buses;
CREATE TRIGGER trigger_set_updated_at
BEFORE UPDATE ON public.buses
FOR EACH ROW
EXECUTE FUNCTION public.handle_updated_at();


-- 2) ROW LEVEL SECURITY (RLS)
-- Enable RLS so table is protected by default
ALTER TABLE public.buses ENABLE ROW LEVEL SECURITY;

-- Allow public (anonymous visitors and authenticated users) to only READ (SELECT)
DROP POLICY IF EXISTS "Public can only read bus locations" ON public.buses;
CREATE POLICY "Public can only read bus locations"
ON public.buses
FOR SELECT
TO anon, authenticated
USING (true);

-- Explicitly disallow public from writing or deleting directly through frontend.
-- Note: Service Role keys automatically bypass RLS for secure backend ingestion.
-- If your backend uses the anon/publishable key to upsert, uncomment the policies below:
--
-- CREATE POLICY "Allow backend insertion"
-- ON public.buses FOR INSERT TO anon WITH CHECK (true);
--
-- CREATE POLICY "Allow backend update"
-- ON public.buses FOR UPDATE TO anon USING (true);


-- 3) ENABLE REALTIME
-- Adds the table to the supabase_realtime publication so the frontend can receive live updates
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1
        FROM pg_publication_tables
        WHERE pubname = 'supabase_realtime'
          AND schemaname = 'public'
          AND tablename = 'buses'
    ) THEN
        ALTER PUBLICATION supabase_realtime ADD TABLE public.buses;
    END IF;
END $$;


-- 4) INSERT STARTER ROW FOR tbeam_001
-- Coordinates are placed at SVNIT Campus, Surat (21.1648, 72.7845)
INSERT INTO public.buses (
    id,
    bus_id,
    device_id,
    lat,
    lng,
    speed,
    heading,
    satellites,
    created_at,
    updated_at
)
VALUES (
    'tbeam_001',
    'BRTS_01',
    'tbeam_001',
    21.164800,
    72.784500,
    26.4,
    85.0,
    9,
    now(),
    now()
)
ON CONFLICT (id) DO UPDATE SET
    lat = EXCLUDED.lat,
    lng = EXCLUDED.lng,
    speed = EXCLUDED.speed,
    heading = EXCLUDED.heading,
    satellites = EXCLUDED.satellites,
    updated_at = now();


-- 5) COMPATIBILITY VIEW FOR bus_locations
-- Ensures any test components querying `bus_locations` will work seamlessly
CREATE OR REPLACE VIEW public.bus_locations AS
SELECT
    id,
    bus_id,
    device_id,
    lat,
    lng,
    speed,
    heading,
    satellites,
    created_at,
    updated_at
FROM public.buses;

GRANT SELECT ON public.bus_locations TO anon, authenticated;
