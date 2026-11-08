import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { defineConfig } from 'vitest/config';

const configDir = dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  resolve: { alias: { '@shared': resolve(configDir, 'src/shared') } },
  test: {
    include: ['src/**/*.test.ts', 'src/**/*.test.tsx', 'mock-host/**/*.test.ts'],
    environment: 'node',
  },
});
