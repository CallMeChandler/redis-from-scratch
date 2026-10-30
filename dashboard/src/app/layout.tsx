import { GeistMono } from "geist/font/mono";
import { GeistSans } from "geist/font/sans";
import type { Metadata } from "next";
import type { ReactNode } from "react";

import { Providers } from "@/components/providers/providers";
import { AppShell } from "@/components/shell/app-shell";
import { THEME_SCRIPT } from "@/components/shell/theme-toggle";

import "./globals.css";

export const metadata: Metadata = {
  title: { default: "Redis From Scratch", template: "%s · Redis From Scratch" },
  description: "Developer playground for a Redis-compatible server written from scratch in C++20.",
};

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en" className={`${GeistSans.variable} ${GeistMono.variable} antialiased`} suppressHydrationWarning>
      <head>
        <script dangerouslySetInnerHTML={{ __html: THEME_SCRIPT }} />
      </head>
      <body>
        <Providers>
          <AppShell>{children}</AppShell>
        </Providers>
      </body>
    </html>
  );
}
