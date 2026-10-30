import { createFileRoute } from "@tanstack/react-router";
import { AppShell } from "@/components/AppShell";
import { PageIntro } from "@/routes/-pageParts";
export const Route = createFileRoute("/transport")({ component: TransportPage });
function TransportPage() { return <AppShell><PageIntro eyebrow="Network" title="Nearby transport" description="Connected public transport information will appear here when a live feed is available." /><div className="empty-state"><div className="empty-icon">—</div><h2>No nearby transport data available</h2><p>There is no public transport feed connected to Rideradar yet.</p></div></AppShell>; }