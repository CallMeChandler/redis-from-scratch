"use client";

import { CheckCircle2, CircleAlert, Info, TriangleAlert, X } from "lucide-react";
import { createContext, useCallback, useContext, useState, type ReactNode } from "react";

import { makeId } from "@/lib/client/id";

export type ToastTone = "success" | "error" | "warning" | "info";

interface Toast {
  id: string;
  tone: ToastTone;
  title: string;
  description?: string;
}

type Notify = (toast: Omit<Toast, "id">) => void;

const ToastContext = createContext<Notify>(() => undefined);

const ICONS = {
  success: <CheckCircle2 className="size-4 text-ok" />,
  error: <CircleAlert className="size-4 text-err" />,
  warning: <TriangleAlert className="size-4 text-warn" />,
  info: <Info className="size-4 text-accent" />,
} satisfies Record<ToastTone, ReactNode>;

export function ToastProvider({ children }: { children: ReactNode }) {
  const [toasts, setToasts] = useState<Toast[]>([]);

  const dismiss = useCallback((id: string) => setToasts((current) => current.filter((toast) => toast.id !== id)), []);

  const notify = useCallback<Notify>(
    (toast) => {
      const id = makeId();
      setToasts((current) => [...current.slice(-3), { ...toast, id }]);
      setTimeout(() => dismiss(id), toast.tone === "error" ? 6000 : 3500);
    },
    [dismiss],
  );

  return (
    <ToastContext.Provider value={notify}>
      {children}
      <div className="pointer-events-none fixed right-4 bottom-4 z-50 flex w-80 flex-col gap-2" aria-live="polite">
        {toasts.map((toast) => (
          <div
            key={toast.id}
            className="animate-slide-in pointer-events-auto flex items-start gap-2.5 rounded-lg border border-border-strong bg-elevated px-3 py-2.5 shadow-pop"
          >
            <span className="mt-0.5">{ICONS[toast.tone]}</span>
            <div className="min-w-0 flex-1">
              <p className="text-sm font-medium text-text">{toast.title}</p>
              {toast.description && <p className="mt-0.5 font-mono text-xs break-words text-muted">{toast.description}</p>}
            </div>
            <button
              type="button"
              onClick={() => dismiss(toast.id)}
              className="rounded p-0.5 text-faint hover:bg-hover hover:text-text"
              aria-label="Dismiss"
            >
              <X className="size-3.5" />
            </button>
          </div>
        ))}
      </div>
    </ToastContext.Provider>
  );
}

export function useToast(): Notify {
  return useContext(ToastContext);
}

