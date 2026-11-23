import { Compartment, EditorState } from '@codemirror/state';
import { EditorView, keymap } from '@codemirror/view';
import { basicSetup } from 'codemirror';
import { html } from '@codemirror/lang-html';
import { json } from '@codemirror/lang-json';
import { StreamLanguage } from '@codemirror/language';
import { oneDark } from '@codemirror/theme-one-dark';

const DSL_KEYWORDS = new Set([
  'let',
  'pipe',
  'use',
  'include',
  'finalize',
  'tee',
  'branch',
  'each',
  'when_exists',
  'unless_exists',
  'when_match',
  'skip_if_exists',
  'skip_if_empty',
  'abort_if_exists',
  'as',
]);

const DSL_ATOMS = new Set(['true', 'false', 'null']);

const DSL_TYPES = new Set([
  'Scraper',
  'ScraperPy',
  'Value',
  'InsertMode',
  'Text',
  'OwnText',
  'Attribute',
  'ElementExists',
  'AttributeExists',
  'Adjacent',
  'Default',
  'Overwrite',
  'AppendToList',
  'UniqueSuffix',
  'Merge',
]);

interface DslState {
  inBlockComment: boolean;
}

export const xsel2Language = StreamLanguage.define<DslState>({
  name: 'xsel2',
  startState: () => ({ inBlockComment: false }),
  token(stream, state) {
    if (state.inBlockComment) {
      if (stream.skipTo('*/')) {
        stream.match('*/');
        state.inBlockComment = false;
      } else {
        stream.skipToEnd();
      }
      return 'comment';
    }

    if (stream.eatSpace()) return null;

    if (stream.match('/*')) {
      state.inBlockComment = true;
      if (stream.skipTo('*/')) {
        stream.match('*/');
        state.inBlockComment = false;
      } else {
        stream.skipToEnd();
      }
      return 'comment';
    }

    if (stream.match('//') || stream.peek() === '#') {
      stream.skipToEnd();
      return 'comment';
    }

    const ch = stream.peek();
    if (ch === '"' || ch === "'") {
      const quote = stream.next();
      let escaped = false;
      while (!stream.eol()) {
        const c = stream.next();
        if (escaped) {
          escaped = false;
        } else if (c === '\\') {
          escaped = true;
        } else if (c === quote) {
          break;
        }
      }
      return 'string';
    }

    if (stream.match(/^(?:->|>>|\?\?|:=|\||\$|@)/)) {
      return 'operator';
    }

    if (stream.match(/^-?\d+(?:\.\d+)?/)) {
      return 'number';
    }

    if (stream.match(/^[a-zA-Z_][a-zA-Z0-9_]*/)) {
      const word = stream.current();
      if (DSL_KEYWORDS.has(word)) return 'keyword';
      if (DSL_ATOMS.has(word)) return 'atom';
      if (DSL_TYPES.has(word)) return 'typeName';
      if (stream.match(/^\s*\(/, false)) return 'function';
      return 'variableName';
    }

    stream.next();
    return null;
  },
});

const lightTheme = EditorView.theme({
  '&': {
    backgroundColor: 'var(--sl-color-bg)',
    color: 'var(--sl-color-text)',
  },
  '.cm-gutters': {
    backgroundColor: 'var(--sl-color-bg-sidebar)',
    color: 'var(--sl-color-gray-3)',
    borderRight: '1px solid var(--sl-color-gray-5)',
  },
});

const sharedLayoutTheme = EditorView.theme({
  '&': {
    height: '100%',
    fontSize: '0.8rem',
    fontFamily: 'var(--sl-font-mono)',
  },
  '.cm-scroller': {
    overflow: 'auto',
    fontFamily: 'var(--sl-font-mono)',
    lineHeight: '1.35',
  },
  '.cm-content': {
    lineHeight: '1.35',
    padding: '4px 0',
  },
  '.cm-line': {
    padding: '0 8px',
    lineHeight: '1.35',
  },
  '.cm-gutters': {
    lineHeight: '1.35',
  },
  '.cm-gutterElement': {
    lineHeight: '1.35',
  },
});

function isDarkTheme(): boolean {
  return document.documentElement.dataset.theme !== 'light';
}

export interface PlaygroundEditors {
  getHtml: () => string;
  setHtml: (value: string) => void;
  getDsl: () => string;
  setDsl: (value: string) => void;
  setOutput: (value: string, isError?: boolean) => void;
}

export function createPlaygroundEditors(
  htmlHost: HTMLElement,
  dslHost: HTMLElement,
  outHost: HTMLElement,
  onRun: () => void
): PlaygroundEditors {
  const themeCompartment = new Compartment();
  const outLangCompartment = new Compartment();

  const currentThemeExt = () => (isDarkTheme() ? oneDark : lightTheme);

  const runKeymap = keymap.of([
    {
      key: 'Mod-Enter',
      run: () => {
        onRun();
        return true;
      },
    },
  ]);

  const htmlView = new EditorView({
    parent: htmlHost,
    state: EditorState.create({
      doc: '',
      extensions: [
        basicSetup,
        html(),
        runKeymap,
        sharedLayoutTheme,
        themeCompartment.of(currentThemeExt()),
      ],
    }),
  });

  const dslView = new EditorView({
    parent: dslHost,
    state: EditorState.create({
      doc: '',
      extensions: [
        basicSetup,
        xsel2Language,
        runKeymap,
        sharedLayoutTheme,
        themeCompartment.of(currentThemeExt()),
      ],
    }),
  });

  const outView = new EditorView({
    parent: outHost,
    state: EditorState.create({
      doc: 'Loading xsel2.wasm...',
      extensions: [
        basicSetup,
        outLangCompartment.of(json()),
        EditorState.readOnly.of(true),
        EditorView.editable.of(false),
        sharedLayoutTheme,
        themeCompartment.of(currentThemeExt()),
      ],
    }),
  });

  const observer = new MutationObserver(() => {
    const ext = currentThemeExt();
    for (const view of [htmlView, dslView, outView]) {
      view.dispatch({ effects: themeCompartment.reconfigure(ext) });
    }
  });
  observer.observe(document.documentElement, {
    attributes: true,
    attributeFilter: ['data-theme'],
  });

  const replaceDoc = (view: EditorView, value: string) => {
    view.dispatch({
      changes: { from: 0, to: view.state.doc.length, insert: value },
    });
  };

  return {
    getHtml: () => htmlView.state.doc.toString(),
    setHtml: (v: string) => replaceDoc(htmlView, v),
    getDsl: () => dslView.state.doc.toString(),
    setDsl: (v: string) => replaceDoc(dslView, v),
    setOutput: (v: string, isError = false) => {
      outHost.dataset.error = isError ? 'true' : 'false';
      outView.dispatch({
        effects: outLangCompartment.reconfigure(isError ? [] : json()),
        changes: { from: 0, to: outView.state.doc.length, insert: v },
      });
    },
  };
}
