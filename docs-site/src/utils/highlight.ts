import { createHighlighter, type Highlighter } from 'shiki';
import xsel2Grammar from '../grammars/xsel2.tmLanguage.json';

let highlighterPromise: Promise<Highlighter> | undefined;

function getHighlighter(): Promise<Highlighter> {
  if (!highlighterPromise) {
    highlighterPromise = createHighlighter({
      themes: ['github-dark', 'github-light'],
      langs: ['html', 'json', 'bash', 'python', 'cpp', 'text', xsel2Grammar],
    });
  }
  return highlighterPromise;
}

export async function highlightCode(code: string, lang: string = 'text'): Promise<string> {
  const highlighter = await getHighlighter();
  const normalized = code.replace(/\r\n/g, '\n').trimEnd();
  const resolvedLang = highlighter.getLoadedLanguages().includes(lang) ? lang : 'text';
  return highlighter.codeToHtml(normalized, {
    lang: resolvedLang,
    themes: {
      light: 'github-light',
      dark: 'github-dark',
    },
    defaultColor: false,
  });
}
