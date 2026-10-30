"use client";

import type { ReactNode } from "react";

import { ActivityProvider } from "./activity-provider";
import { HealthProvider } from "./health-provider";
import { ToastProvider } from "./toast-provider";

export function Providers({ children }: { children: ReactNode }) {
  return (
    <ToastProvider>
      <HealthProvider>
        <ActivityProvider>{children}</ActivityProvider>
      </HealthProvider>
    </ToastProvider>
  );
}
