export interface CliFlagInfo {
  isSelectorMode: boolean;
  scraperName: string | null;
}

interface CliToken {
  raw: string;
  value: string;
  start: number;
  end: number;
}

/**
 * Replaces comments (//, #, /* *\/) and string literals ("...", '...', """...""", '''...''')
 * with spaces while preserving newlines so line-start anchors remain accurate.
 */
function stripCommentsAndStrings(dsl: string): string {
  const out: string[] = [];
  let i = 0;
  const n = dsl.length;

  while (i < n) {
    const ch = dsl[i];
    const next = dsl[i + 1];

    // Block comment: /* ... */
    if (ch === '/' && next === '*') {
      out.push(' ', ' ');
      i += 2;
      while (i < n && !(dsl[i] === '*' && dsl[i + 1] === '/')) {
        out.push(dsl[i] === '\n' ? '\n' : ' ');
        i++;
      }
      if (i < n) {
        out.push(' ', ' ');
        i += 2;
      }
      continue;
    }

    // Line comment: // ... or # ...
    if ((ch === '/' && next === '/') || ch === '#') {
      while (i < n && dsl[i] !== '\n') {
        out.push(' ');
        i++;
      }
      continue;
    }

    // String literal: """...""", '''...''', "...", '...'
    if (ch === '"' || ch === "'") {
      const isTriple = dsl[i + 1] === ch && dsl[i + 2] === ch;
      const delim = isTriple ? ch.repeat(3) : ch;
      for (let k = 0; k < delim.length; k++) out.push(' ');
      i += delim.length;

      while (i < n) {
        if (!isTriple && dsl[i] === '\\' && i + 1 < n) {
          out.push(' ', dsl[i + 1] === '\n' ? '\n' : ' ');
          i += 2;
          continue;
        }
        if (isTriple) {
          if (dsl.startsWith(delim, i)) {
            for (let k = 0; k < delim.length; k++) out.push(' ');
            i += delim.length;
            break;
          }
        } else if (dsl[i] === ch) {
          out.push(' ');
          i++;
          break;
        } else if (dsl[i] === '\n') {
          // Unterminated single-line string
          break;
        }
        out.push(dsl[i] === '\n' ? '\n' : ' ');
        i++;
      }
      continue;
    }

    out.push(ch);
    i++;
  }

  return out.join('');
}

/**
 * Extracts unique top-level `<name> = Scraper(...)` identifiers from DSL source,
 * preserving first-seen order and ignoring comments and string literals.
 */
export function extractScrapers(dsl: string): string[] {
  const cleaned = stripCommentsAndStrings(dsl);
  const re = /^[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]*=[ \t]*Scraper[ \t]*\(/gm;
  const seen = new Set<string>();
  const scrapers: string[] = [];

  for (const match of cleaned.matchAll(re)) {
    const name = match[1];
    if (!seen.has(name)) {
      seen.add(name);
      scrapers.push(name);
    }
  }

  return scrapers;
}

function unquoteToken(raw: string): string {
  if (raw.length >= 2) {
    const first = raw[0];
    const last = raw[raw.length - 1];
    if ((first === '"' && last === '"') || (first === "'" && last === "'")) {
      return raw.slice(1, -1);
    }
  }
  return raw;
}

function tokenizeCli(cmd: string): CliToken[] {
  const tokens: CliToken[] = [];
  let i = 0;
  const n = cmd.length;

  while (i < n) {
    while (i < n && /\s/.test(cmd[i])) i++;
    if (i >= n) break;

    const start = i;
    let value = '';
    let quote: '"' | "'" | null = null;

    while (i < n) {
      const ch = cmd[i];
      if (quote) {
        if (quote === '"' && ch === '\\' && i + 1 < n) {
          value += cmd[i + 1];
          i += 2;
          continue;
        }
        if (ch === quote) {
          quote = null;
          i++;
          continue;
        }
        value += ch;
        i++;
      } else {
        if (/\s/.test(ch)) break;
        if (ch === '"' || ch === "'") {
          quote = ch;
          i++;
          continue;
        }
        if (ch === '\\' && i + 1 < n) {
          value += cmd[i + 1];
          i += 2;
          continue;
        }
        value += ch;
        i++;
      }
    }

    tokens.push({
      raw: cmd.slice(start, i),
      value,
      start,
      end: i,
    });
  }

  return tokens;
}

/**
 * Parses CLI flags to determine whether `-s`/`--selector` mode is active
 * and which `-n`/`--name` scraper is currently selected.
 */
export function parseCliFlags(cmd: string): CliFlagInfo {
  const tokens = tokenizeCli(cmd);
  let isSelectorMode = false;
  let scraperName: string | null = null;

  for (let i = 0; i < tokens.length; i++) {
    const tok = tokens[i].value;
    if (tok === '-s' || tok === '--selector' || tok.startsWith('--selector=')) {
      isSelectorMode = true;
      continue;
    }
    if (tok === '-n' || tok === '--name') {
      if (i + 1 < tokens.length) {
        scraperName = tokens[i + 1].value;
        i++;
      }
      continue;
    }
    if (tok.startsWith('-n=')) {
      scraperName = unquoteToken(tok.slice(3));
      continue;
    }
    if (tok.startsWith('--name=')) {
      scraperName = unquoteToken(tok.slice(7));
    }
  }

  return { isSelectorMode, scraperName };
}

/**
 * Updates or inserts `-n <name>` in a CLI command string without disturbing
 * quoted arguments or `-s` quick-selector commands.
 */
export function setCliScraper(cmd: string, name: string): string {
  const tokens = tokenizeCli(cmd);
  for (const t of tokens) {
    if (t.value === '-s' || t.value === '--selector' || t.value.startsWith('--selector=')) {
      return cmd;
    }
  }

  for (let i = 0; i < tokens.length; i++) {
    const tok = tokens[i];
    if (tok.value === '-n' || tok.value === '--name') {
      if (i + 1 < tokens.length) {
        const valTok = tokens[i + 1];
        return `${cmd.slice(0, tok.start)}-n ${name}${cmd.slice(valTok.end)}`;
      }
      return `${cmd.slice(0, tok.start)}-n ${name}${cmd.slice(tok.end)}`;
    }
    if (tok.value.startsWith('-n=') || tok.value.startsWith('--name=')) {
      return `${cmd.slice(0, tok.start)}-n ${name}${cmd.slice(tok.end)}`;
    }
  }

  const htmlTok = tokens.find(
    (t) => t.value === '--html' || t.value.startsWith('--html=')
  );
  if (htmlTok) {
    return `${cmd.slice(0, htmlTok.start)}-n ${name} ${cmd.slice(htmlTok.start)}`;
  }

  const trimmed = cmd.trimEnd();
  return trimmed ? `${trimmed} -n ${name}` : `-n ${name}`;
}
