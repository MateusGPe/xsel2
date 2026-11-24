import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import mermaid from 'astro-mermaid';
import remarkGithubBlockquoteAlert from 'remark-github-blockquote-alert';
import fs from 'node:fs';

const xsel2Grammar = JSON.parse(
  fs.readFileSync(new URL('./src/grammars/xsel2.tmLanguage.json', import.meta.url), 'utf8')
);

export default defineConfig({
  site: 'https://MateusGPe.github.io',
  base: '/xsel2',
  markdown: {
    remarkPlugins: [remarkGithubBlockquoteAlert],
  },
  integrations: [
    mermaid({
      autoTheme: true,
    }),
    starlight({
      title: 'xsel2',
      description:
        'Declarative, compiled HTML/XML & JSON extraction pipelines in C++20, Python, and WebAssembly.',
      expressiveCode: {
        shiki: {
          langs: [xsel2Grammar],
        },
      },
      social: {
        github: 'https://github.com/MateusGPe/xsel2',
      },
      customCss: ['./src/styles/custom.css'],
      sidebar: [
        {
          label: 'Getting Started',
          items: [
            { label: 'Quickstart', slug: 'quickstart' },
          ],
        },
        {
          label: 'DSL Manual',
          items: [
            { label: 'Mental Model & Execution', slug: 'dsl/mental-model' },
            { label: 'Grammar & Control Flow', slug: 'dsl/grammar-and-control-flow' },
            { label: 'Pipeline Steps Reference', slug: 'dsl/steps-reference' },
          ],
        },
        {
          label: 'Interfaces',
          items: [
            { label: 'CLI (xsel2)', slug: 'cli' },
            { label: 'Python (scraperpy)', slug: 'python' },
            { label: 'C++20 API', slug: 'cpp-api' },
          ],
        },
        {
          label: 'Interactive',
          items: [
            { label: 'WASM Playground', slug: 'playground' },
          ],
        },
      ],
    }),
  ],
});
