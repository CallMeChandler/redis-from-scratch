"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";

import { cn } from "@/lib/cn";

import { NAV_GROUPS, NAV_ITEMS } from "./nav-items";

export function Sidebar({ open, onNavigate }: { open: boolean; onNavigate: () => void }) {
  const pathname = usePathname();

  return (
    <>
      <div
        className={cn("fixed inset-0 top-12 z-30 bg-black/50 md:hidden", open ? "block" : "hidden")}
        onClick={onNavigate}
        aria-hidden
      />
      <nav
        aria-label="Sections"
        className={cn(
          "fixed top-12 bottom-0 left-0 z-40 w-56 shrink-0 overflow-y-auto border-r border-border bg-panel px-2.5 py-4 transition-transform md:static md:translate-x-0",
          open ? "translate-x-0" : "-translate-x-full",
        )}
      >
        {NAV_GROUPS.map((group) => (
          <div key={group} className="mb-4">
            <p className="mb-1 px-2.5 text-[10px] font-semibold tracking-[0.08em] text-faint uppercase">{group}</p>
            <ul className="space-y-px">
              {NAV_ITEMS.filter((item) => item.group === group).map((item) => {
                const active = item.href === "/" ? pathname === "/" : pathname.startsWith(item.href);
                const Icon = item.icon;
                return (
                  <li key={item.href}>
                    <Link
                      href={item.href}
                      onClick={onNavigate}
                      aria-current={active ? "page" : undefined}
                      className={cn(
                        "flex items-center gap-2.5 rounded-md px-2.5 py-1.5 text-[13px] transition-colors",
                        active ? "bg-elevated font-medium text-text" : "text-muted hover:bg-hover hover:text-text",
                      )}
                    >
                      <Icon className={cn("size-4", active ? "text-accent" : "text-faint")} />
                      {item.label}
                    </Link>
                  </li>
                );
              })}
            </ul>
          </div>
        ))}
        <div className="mt-6 border-t border-border px-2.5 pt-3 text-[11px] leading-relaxed text-faint">
          Every request is hand-encoded RESP2 over a raw TCP socket from the Node bridge.
        </div>
      </nav>
    </>
  );
}
