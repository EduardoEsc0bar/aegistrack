/** @type {import('next').NextConfig} */
const nextConfig = {
  outputFileTracingRoot: new URL(".", import.meta.url).pathname,
  transpilePackages: ["cesium"],
  typedRoutes: true
};

export default nextConfig;
