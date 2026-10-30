import { useEffect, useState } from "react";
import { supabase, supabaseConfigured } from "@/lib/supabase";
import type { TrackerLocation, TrackerStatus } from "@/types/telemetry";

export const TRACKER_STALE_THRESHOLD_MS = 90_000;
type TrackerState = { location: TrackerLocation | null; status: TrackerStatus; loading: boolean; error: string | null };

function parseLocation(row: Record<string, unknown>): TrackerLocation | null {
  const latitude = Number(row.lat);
  const longitude = Number(row.lng);
  if (!Number.isFinite(latitude) || !Number.isFinite(longitude)) return null;
  return {
    id: String(row.id ?? "tbeam_001"),
    latitude,
    longitude,
    receivedAt: typeof row.updated_at === "string" ? row.updated_at : new Date().toISOString(),
    speed: row.speed !== undefined && row.speed !== null ? Number(row.speed) : null,
    satellites: row.satellites !== undefined && row.satellites !== null ? Number(row.satellites) : null,
    heading: row.heading !== undefined && row.heading !== null ? Number(row.heading) : null,
  };
}

export function useSupabaseTracker(): TrackerState {
  const [state, setState] = useState<TrackerState>({ location: null, status: "CONNECTING", loading: true, error: null });
  useEffect(() => {
    const client = supabase;
    if (!supabaseConfigured || !client) {
      setState({ location: null, status: "OFFLINE", loading: false, error: "Supabase is not configured." });
      return;
    }
    let mounted = true;
    const applyRows = (rows: unknown[]) => {
      const next = rows.map((row) => parseLocation(row as Record<string, unknown>)).find(Boolean) ?? null;
      if (mounted) {
        setState((previous) => {
          const unchanged = Boolean(
            previous.location &&
            next &&
            previous.location.id === next.id &&
            previous.location.latitude === next.latitude &&
            previous.location.longitude === next.longitude
          );
          let location: TrackerLocation | null = next;
          if (unchanged && previous.location && next) {
            location = { ...next, receivedAt: previous.location.receivedAt };
          }
          return { location, status: location ? "LIVE" : "OFFLINE", loading: false, error: null };
        });
      }
    };
    const load = async () => {
      const { data, error } = await client
        .from("buses")
        .select("id, lat, lng, speed, satellites, heading, updated_at")
        .order("updated_at", { ascending: false })
        .limit(1);
      if (!mounted) return;
      if (error) {
        console.error("Unable to read buses from Supabase", error);
        setState({ location: null, status: "OFFLINE", loading: false, error: "Unable to retrieve the latest tracker location." });
        return;
      }
      applyRows(data ?? []);
    };
    void load();
    const interval = window.setInterval(() => void load(), 10_000);
    const channel = client.channel("buses-tracker").on("postgres_changes", { event: "*", schema: "public", table: "buses" }, (payload) => {
      const next = parseLocation((payload.new ?? {}) as Record<string, unknown>);
      if (mounted && next) setState({ location: next, status: "LIVE", loading: false, error: null });
    }).subscribe();
    return () => {
      mounted = false;
      window.clearInterval(interval);
      void client.removeChannel(channel);
    };
  }, []);
  const isStale = Boolean(state.location && Date.now() - Date.parse(state.location.receivedAt) > TRACKER_STALE_THRESHOLD_MS);
  return isStale ? { ...state, status: "OFFLINE" } : state;
}