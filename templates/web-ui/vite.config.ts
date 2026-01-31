import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],

  // Relative paths for local file loading in WebView
  base: './',

  build: {
    // Output to dist/ (CMake copies this to Resources/WebUI)
    outDir: 'dist',

    // Clean output directory before build
    emptyOutDir: true,

    // Predictable asset naming for resource provider
    rollupOptions: {
      output: {
        entryFileNames: 'assets/index.js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name].[ext]',
      },
    },
  },

  server: {
    // Match the DEV_SERVER_URL in PluginEditor.cpp
    port: 5173,
    strictPort: true,

    // Allow access from WebView
    cors: true,
  },
});
