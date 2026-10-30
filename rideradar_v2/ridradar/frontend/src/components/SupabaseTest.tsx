import { useEffect, useState } from "react";
import { supabase } from "../lib/supabase";

type BusLocation = {
  id: string;
  bus_id: string;
  device_id: string | null;
  lat: number;
  lng: number;
  speed: number | null;
  heading: number | null;
  satellites: number | null;
  created_at: string;
};

export default function SupabaseTest() {
  const [row, setRow] = useState<BusLocation | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    async function fetchLocation() {
      if (!supabase) {
        setError("Supabase is not configured.");
        setLoading(false);
        return;
      }

      const { data, error } = await supabase
        .from("bus_locations")
        .select("*")
        .order("created_at", { ascending: false })
        .limit(1)
        .maybeSingle();

      if (error) {
        setError(error.message);
        setLoading(false);
        return;
      }

      setRow(data as BusLocation | null);
      setLoading(false);
    }

    fetchLocation();
  }, []);

  if (loading) return <p>Loading location from Supabase...</p>;

  if (error) {
    return (
      <div>
        <h2>Supabase connection failed</h2>
        <pre>{error}</pre>
      </div>
    );
  }

  if (!row) return <p>No readable rows found in bus_locations.</p>;

  return (
    <main style={{ padding: 24, fontFamily: "sans-serif" }}>
      <h2>Rideradar — Supabase Connection Test</h2>
      <p>Successfully read a row from bus_locations.</p>

      <pre
        style={{
          padding: 16,
          background: "#eee",
          color: "#111",
          borderRadius: 8,
          overflowX: "auto",
        }}
      >
        {JSON.stringify(row, null, 2)}
      </pre>

      <h3>GPS coordinates</h3>
      <p>Latitude: {row.lat}</p>
      <p>Longitude: {row.lng}</p>
      <p>Device ID: {row.device_id ?? "Unavailable"}</p>
      <p>Last recorded: {row.created_at}</p>
    </main>
  );
}
