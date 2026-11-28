import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import {
  extractScrapers,
  parseCliFlags,
  setCliScraper,
} from '../src/components/xsel2-cli-sync.ts';

describe('extractScrapers', () => {
  it('extracts single and multiple Scraper() definitions in order', () => {
    const dsl = `
let base = "https://example.com"

dom_and_text = Scraper()
  | $("h1", "title")

urls_dates_unicode = Scraper()
  | $("a", "link")

compact_one=Scraper( )
  | $("p", "body")
`;
    assert.deepEqual(extractScrapers(dsl), [
      'dom_and_text',
      'urls_dates_unicode',
      'compact_one',
    ]);
  });

  it('deduplicates repeated scraper names while preserving first-seen order', () => {
    const dsl = `
first = Scraper()
second = Scraper()
first = Scraper()
`;
    assert.deepEqual(extractScrapers(dsl), ['first', 'second']);
  });

  it('ignores Scraper() definitions inside //, #, and /* */ comments', () => {
    const dsl = `
// commented_out = Scraper()
# hash_comment = Scraper()
/*
  block_comment = Scraper()
*/
active_scraper = Scraper() // inline comment = Scraper()
  | $("div")
`;
    assert.deepEqual(extractScrapers(dsl), ['active_scraper']);
  });

  it('ignores Scraper() patterns inside string literals', () => {
    const dsl = `
let sample = "fake_in_string = Scraper()"
let multi = """
another_fake = Scraper()
"""
real_scraper = Scraper()
`;
    assert.deepEqual(extractScrapers(dsl), ['real_scraper']);
  });

  it('returns an empty array when no scrapers are defined', () => {
    assert.deepEqual(extractScrapers('let x = 1\n// nothing here'), []);
  });
});

describe('parseCliFlags', () => {
  it('extracts -n <name>, -n=<name>, --name <name>, and --name=<name>', () => {
    assert.deepEqual(parseCliFlags('xsel2 -d a.dsl -n product --html a.html'), {
      isSelectorMode: false,
      scraperName: 'product',
    });
    assert.deepEqual(parseCliFlags('xsel2 -d a.dsl -n=product --html a.html'), {
      isSelectorMode: false,
      scraperName: 'product',
    });
    assert.deepEqual(parseCliFlags('xsel2 -d a.dsl --name summary --html a.html'), {
      isSelectorMode: false,
      scraperName: 'summary',
    });
    assert.deepEqual(parseCliFlags('xsel2 -d a.dsl --name="summary" --html a.html'), {
      isSelectorMode: false,
      scraperName: 'summary',
    });
  });

  it('detects -s / --selector mode', () => {
    assert.deepEqual(parseCliFlags('xsel2 -s "ul.specs li" --html sample.html'), {
      isSelectorMode: true,
      scraperName: null,
    });
    assert.deepEqual(parseCliFlags('xsel2 --selector "h1" --html sample.html'), {
      isSelectorMode: true,
      scraperName: null,
    });
  });

  it('does not confuse -ndjson, -c, or quoted -n/-s inside values', () => {
    assert.deepEqual(
      parseCliFlags('xsel2 -d a.dsl --ndjson --html "my -n -s file.html" -c'),
      {
        isSelectorMode: false,
        scraperName: null,
      }
    );
    assert.deepEqual(
      parseCliFlags('xsel2 -d a.dsl --html "foo -n bar.html" -n actual_name'),
      {
        isSelectorMode: false,
        scraperName: 'actual_name',
      }
    );
  });
});

describe('setCliScraper', () => {
  it('replaces an existing -n <name> or --name=<name> in place', () => {
    assert.equal(
      setCliScraper('xsel2 -d card.dsl -n product --html card.html', 'summary'),
      'xsel2 -d card.dsl -n summary --html card.html'
    );
    assert.equal(
      setCliScraper('xsel2 -d card.dsl --name=product --html card.html -c', 'summary'),
      'xsel2 -d card.dsl -n summary --html card.html -c'
    );
  });

  it('inserts -n <name> before --html when -n is omitted', () => {
    assert.equal(
      setCliScraper('xsel2 -d card.dsl --html card.html -c', 'summary'),
      'xsel2 -d card.dsl -n summary --html card.html -c'
    );
  });

  it('appends -n <name> at the end when neither -n nor --html is present', () => {
    assert.equal(
      setCliScraper('xsel2 -d card.dsl -c', 'summary'),
      'xsel2 -d card.dsl -c -n summary'
    );
  });

  it('does not corrupt quoted strings containing -n and leaves -s commands untouched', () => {
    assert.equal(
      setCliScraper('xsel2 -d a.dsl --html "my -n file.html"', 'chosen'),
      'xsel2 -d a.dsl -n chosen --html "my -n file.html"'
    );
    assert.equal(
      setCliScraper('xsel2 -s "div.item" --html sample.html', 'chosen'),
      'xsel2 -s "div.item" --html sample.html'
    );
  });
});
