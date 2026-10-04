import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

const apiTarget = process.env.VITE_API_PROXY ?? 'http://localhost:5181';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5180,
    host: true,
    proxy: {
      '/api': apiTarget,
      '/hubs': { target: apiTarget, ws: true },
    },
  },
});
