import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  reactCompiler: true,
  // The dashboard is served by the custom long-lived server in server.ts.
  poweredByHeader: false,
  // Let `next dev` HMR work when the dashboard is opened via 127.0.0.1 as well as localhost.
  allowedDevOrigins: ["127.0.0.1"],
  turbopack: {
    rules: {
      "*.css": {
        loaders: ["@tailwindcss/turbopack"],
        as: "*.css",
      },
    },
  },
};

export default nextConfig;
