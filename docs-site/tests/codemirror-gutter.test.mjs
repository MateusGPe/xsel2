import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

const root = resolve(import.meta.dirname, '..');
const playgroundAstro = readFileSync(
  resolve(root, 'src/components/XselPlayground.astro'),
  'utf8'
);
const codemirrorTs = readFileSync(
  resolve(root, 'src/components/xsel2-codemirror.ts'),
  'utf8'
);

/**
 * CodeMirror 6 virtualizes gutters by setting inline `style.marginTop` on the
 * first rendered `.cm-gutterElement` when scrolled. Overriding `margin` or
 * `margin-top` (especially with `!important`) breaks virtual-scroll alignment
 * and causes line numbers to stop rendering partway down the viewport.
 */
describe('Playground CodeMirror gutter styling', () => {
  it('does not override margin on .cm-gutterElement in XselPlayground.astro', () => {
    const match = playgroundAstro.match(
      /\.cm-gutterElement\)\s*\{([^}]*)\}/
    );
    assert.ok(match, 'Expected .cm-gutterElement rule in XselPlayground.astro');
    const ruleBody = match[1];
    assert.doesNotMatch(
      ruleBody,
      /\bmargin(?:-top)?\s*:/i,
      'XselPlayground.astro must not set margin/margin-top on .cm-gutterElement'
    );
    assert.match(
      ruleBody,
      /\bline-height\s*:\s*1\.4\b/i,
      'Expected line-height: 1.4 to be preserved on .cm-gutterElement'
    );
  });

  it('does not override margin on .cm-gutterElement in xsel2-codemirror.ts', () => {
    const match = codemirrorTs.match(
      /['"]\.cm-gutterElement['"]\s*:\s*\{([^}]*)\}/
    );
    assert.ok(match, 'Expected .cm-gutterElement theme entry in xsel2-codemirror.ts');
    const ruleBody = match[1];
    assert.doesNotMatch(
      ruleBody,
      /\bmargin(?:Top)?\s*:/i,
      'xsel2-codemirror.ts must not set margin/marginTop on .cm-gutterElement'
    );
    assert.match(
      ruleBody,
      /\blineHeight\s*:\s*['"]1\.4['"]/,
      'Expected lineHeight: "1.4" to be preserved on .cm-gutterElement'
    );
  });
});
