import { AlertCircle, CheckCircle2, Clock3, Crosshair, Gauge, Satellite } from "lucide-react";
import type { ReactNode } from "react";
import type { TrackerLocation, TrackerStatus } from "@/types/telemetry";

function statusCopy(status: TrackerStatus) {
  return status === "LIVE"
    ? "Receiving real-time GPS telemetry"
    : status === "CONNECTING"
    ? "Connecting to tracker..."
    : "No recent GPS update (Tracker offline)";
}

export function TrackerPanel({
  location,
  status,
  error,
}: {
  location: TrackerLocation | null;
  status: TrackerStatus;
  error: string | null;
}) {
  const statusIcon =
    status === "LIVE" ? (
      <CheckCircle2 size={17} />
    ) : status === "CONNECTING" ? (
      <Clock3 size={17} />
    ) : (
      <AlertCircle size={17} />
    );

  return (
    <aside className="tracker-panel">
      <div className="panel-heading">
        <div>
          <p className="eyebrow">Primary device</p>
          <h2>T-Beam Tracker</h2>
        </div>
        <span className={`status status-${status.toLowerCase()}`}>
          {statusIcon}
          {status}
        </span>
      </div>
      <p className="status-copy">{error ?? statusCopy(status)}</p>

      <section className="location-block">
        <p className="field-label">Current location</p>
        {location ? (
          <p className="coordinates">
            {location.latitude.toFixed(6)}, {location.longitude.toFixed(6)}
          </p>
        ) : (
          <p className="empty-value">Unavailable</p>
        )}
      </section>

      <div className="telemetry-grid">
        <Telemetry
          icon={<Clock3 size={16} />}
          label="Last updated"
          value={location ? new Date(location.receivedAt).toLocaleTimeString() : "Unavailable"}
        />
        <Telemetry
          icon={<Crosshair size={16} />}
          label="Device ID"
          value={location?.id ?? "Unavailable"}
        />
        <Telemetry
          icon={<Gauge size={16} />}
          label="Speed"
          value={location?.speed != null ? `${location.speed} km/h` : "Unavailable"}
        />
        <Telemetry
          icon={<Satellite size={16} />}
          label="Satellites"
          value={location?.satellites != null ? `${location.satellites} locked` : "Unavailable"}
        />
      </div>

      <p className="data-note">
        Direct live link to Supabase Realtime telemetry stream.
      </p>
    </aside>
  );
}

function Telemetry({ icon, label, value }: { icon: ReactNode; label: string; value: string }) {
  return (
    <div className="telemetry-item">
      <span className="telemetry-icon">{icon}</span>
      <div>
        <p className="field-label">{label}</p>
        <p className="telemetry-value">{value}</p>
      </div>
    </div>
  );
}