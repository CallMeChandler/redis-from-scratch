"use client";

import { useState, type ReactNode } from "react";

import { EventLog } from "./event-log";
import { InspectorDrawer } from "./inspector-drawer";
import { Sidebar } from "./sidebar";
import { TopBar } from "./top-bar";

export function AppShell({ children }: { children: ReactNode }) {
  const [menuOpen, setMenuOpen] = useState(false);

  return (
    <div className="flex h-dvh flex-col">
      <TopBar onMenu={() => setMenuOpen(!menuOpen)} />
      <div className="flex min-h-0 flex-1">
        <Sidebar open={menuOpen} onNavigate={() => setMenuOpen(false)} />
        <div className="flex min-w-0 flex-1 flex-col">
          <main className="min-h-0 flex-1 overflow-y-auto">
            <div className="mx-auto max-w-[1440px] px-4 py-6 md:px-6">{children}</div>
          </main>
          <EventLog />
        </div>
      </div>
      <InspectorDrawer />
    </div>
  );
}
