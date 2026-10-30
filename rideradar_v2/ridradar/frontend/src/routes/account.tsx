import { createFileRoute } from "@tanstack/react-router";
import { AppShell } from "@/components/AppShell";
import { PageIntro } from "@/routes/-pageParts";
export const Route = createFileRoute("/account")({ component: AccountPage });
function AccountPage() { return <AppShell><PageIntro eyebrow="Workspace" title="Account" description="Account controls will become available when authentication is connected." /><div className="settings-list">{["Profile", "Preferences", "Notifications", "Privacy", "Help"].map((item) => <div className="setting-row" key={item}><span>{item}</span><span className="unavailable">Unavailable</span></div>)}</div></AppShell>; }