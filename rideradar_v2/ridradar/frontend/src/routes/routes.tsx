import { createFileRoute } from "@tanstack/react-router";
import { AppShell } from "@/components/AppShell";
import { PageIntro } from "@/routes/-pageParts";
import { Search } from "lucide-react";
export const Route = createFileRoute("/routes")({ component: RoutesPage });
function RoutesPage() { return <AppShell><PageIntro eyebrow="Planning" title="Find a route" description="Search is ready for a routing provider when live route data is connected." /><div className="route-search"><div className="search-box"><Search size={19} /><input aria-label="Search destination" placeholder="Search destination" /></div><div className="route-empty"><h2>Routing unavailable</h2><p>Live route results, travel times, and transport modes will appear here when a routing service is connected.</p></div></div></AppShell>; }