import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: './', // Relative paths for local file loading
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    // Inline small assets to reduce file count
    assetsInlineLimit: 4096,
    // Single chunk for simplicity
    rollupOptions: {
      output: {
        manualChunks: undefined,
      },
    },
  },
  server: {
    port: 5173,
    // Allow access from WebView
    cors: true,
  },
})
