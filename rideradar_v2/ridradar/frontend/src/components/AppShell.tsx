import { Link, useRouterState } from "@tanstack/react-router";
import { Map, Navigation, Search, UserRound, Radio } from "lucide-react";
import type { ReactNode } from "react";

const links = [{ to: "/tracking", label: "Tracking", icon: Map }, { to: "/transport", label: "Nearby transport", icon: Navigation }, { to: "/routes", label: "Routes", icon: Search }, { to: "/account", label: "Account", icon: UserRound }] as const;
export function AppShell({ children }: { children: ReactNode }) {
  const pathname = useRouterState({ select: (state) => state.location.pathname });
  return <div className="app-shell"><header className="topbar"><Link to="/tracking" className="brand"><span className="brand-mark"><Radio size={18} /></span><span>Rideradar</span></Link><nav className="desktop-nav" aria-label="Primary navigation">{links.map(({ to, label, icon: Icon }) => <Link key={to} to={to} className={pathname === to ? "nav-link active" : "nav-link"}><Icon size={17} />{label}</Link>)}</nav><div className="system-label"><span className="status-dot live" />Device network</div></header><main className="app-main">{children}</main><nav className="mobile-nav" aria-label="Mobile navigation">{links.map(({ to, label, icon: Icon }) => <Link key={to} to={to} className={pathname === to ? "mobile-link active" : "mobile-link"}><Icon size={18} /><span>{label}</span></Link>)}</nav></div>;
}