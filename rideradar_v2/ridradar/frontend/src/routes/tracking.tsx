import { createFileRoute } from "@tanstack/react-router";
import { AppShell } from "@/components/AppShell";
import { TrackerMap } from "@/components/TrackerMap";
import { TrackerPanel } from "@/components/TrackerPanel";
import { useSupabaseTracker } from "@/hooks/useSupabaseTracker";
export const Route = createFileRoute("/tracking")({ component: TrackerPage });
export function TrackerPage() { const tracker = useSupabaseTracker(); return <AppShell><div className="page-header"><div><p className="eyebrow">Live operations</p><h1>Tracking overview</h1><p className="page-description">Monitor the LilyGO T-Beam location in Surat.</p></div><div className="location-chip"><span className="status-dot live" />Surat, Gujarat</div></div><div className="tracking-layout"><TrackerMap location={tracker.location} /><TrackerPanel location={tracker.location} status={tracker.status} error={tracker.error} /></div></AppShell>; }