// =============================================================================
// xsel2 - Exhaustive DSL for HTML Metadata, JSON, and Schema Extraction
// =============================================================================
// Extracts standard HTML metadata, http-equiv, Open Graph (core, article, book,
// profile, music, video, product), Twitter/X Cards (incl. Slack/Discord),
// Facebook, Instagram & App Links, Dublin Core, Google Scholar, Parse.ly,
// Sailthru, Apple/iOS, Microsoft/Windows, Pinterest, Domain Verification,
// <link> relations (SEO, Feeds, IndieWeb, Fediverse, Monetization), JSON-LD,
// modern framework JSONs/states (Next.js, Nuxt, Remix, Angular, SvelteKit,
// Shopify, importmap, speculationrules), Microdata, RDFa, and generic fallback
// arrays (all_meta, all_links).
// =============================================================================

metadata = Scraper()
  // ---------------------------------------------------------------------------
  // 1. Basic HTML
  // ---------------------------------------------------------------------------
  | $("html", "basic") {
      @lang | clean -> "lang"
      | @dir | clean -> "dir"
    }
  | $("head title", "basic")                 { text | clean -> "title" }
  | $("meta[charset]", "basic")              { @charset | lower | clean -> "charset" }
  | $("meta[name='description']", "basic")   { @content | clean -> "description" }
  | $("meta[name='keywords']", "basic")      { @content | clean -> "keywords" }
  | $("meta[name='news_keywords']", "basic") { @content | clean -> "news_keywords" }
  | $("meta[name='author']", "basic")        { @content | clean -> "author" }
  | $("meta[name='creator']", "basic")       { @content | clean -> "creator" }
  | $("meta[name='publisher']", "basic")     { @content | clean -> "publisher" }
  | $("meta[name='generator']", "basic")     { @content | clean -> "generator" }
  | $("meta[name='application-name']", "basic") { @content | clean -> "application_name" }
  | $("meta[name='robots']", "basic")        { @content | clean -> "robots" }
  | $("meta[name='googlebot']", "basic")     { @content | clean -> "googlebot" }
  | $("meta[name='bingbot']", "basic")       { @content | clean -> "bingbot" }
  | $("meta[name='viewport']", "basic")      { @content | clean -> "viewport" }
  | $("meta[name='theme-color']", "basic")   { @content | clean -> "theme_color" }
  | $("meta[name='color-scheme']", "basic")  { @content | clean -> "color_scheme" }
  | $("meta[name='referrer']", "basic")      { @content | clean -> "referrer" }
  | $("meta[name='rating']", "basic")        { @content | clean -> "rating" }
  | $("meta[name='format-detection']", "basic") { @content | clean -> "format_detection" }
  | $("meta[name='copyright']", "basic")     { @content | clean -> "copyright" }
  | $("meta[name='language']", "basic")      { @content | clean -> "language" }
  | $("meta[name='revisit-after']", "basic") { @content | clean -> "revisit_after" }
  | $("meta[name='distribution']", "basic")  { @content | clean -> "distribution" }
  | $("meta[name='syndication-source']", "basic") { @content | clean -> "syndication_source" }
  | $("meta[name='original-source']", "basic")    { @content | clean -> "original_source" }
  | $("meta[name='standout']", "basic")      { @content | clean -> "standout" }
  | $("meta[name='geo.region']", "basic")    { @content | clean -> "geo_region" }
  | $("meta[name='geo.placename']", "basic") { @content | clean -> "geo_placename" }
  | $("meta[name='geo.position']", "basic")  { @content | clean -> "geo_position" }
  | $("meta[name='ICBM']", "basic")          { @content | clean -> "icbm" }
  | $("meta[name='fediverse:creator'], meta[property='fediverse:creator']", "basic") { @content | clean -> "fediverse_creator" }
  | $("meta[name='telegram:channel'], meta[property='telegram:channel']", "basic")   { @content | clean -> "telegram_channel" }

  // ---------------------------------------------------------------------------
  // 2. http-equiv Directives
  // ---------------------------------------------------------------------------
  | $("meta[http-equiv='content-type'], meta[http-equiv='Content-Type']", "http_equiv")                       { @content | clean -> "content_type" }
  | $("meta[http-equiv='content-language'], meta[http-equiv='Content-Language']", "http_equiv")               { @content | clean -> "content_language" }
  | $("meta[http-equiv='refresh'], meta[http-equiv='Refresh']", "http_equiv")                                 { @content | clean -> "refresh" }
  | $("meta[http-equiv='Content-Security-Policy'], meta[http-equiv='content-security-policy']", "http_equiv") { @content | clean -> "content_security_policy" }
  | $("meta[http-equiv='X-UA-Compatible'], meta[http-equiv='x-ua-compatible']", "http_equiv")                 { @content | clean -> "x_ua_compatible" }
  | $("meta[http-equiv='cache-control'], meta[http-equiv='Cache-Control']", "http_equiv")                     { @content | clean -> "cache_control" }

  // ---------------------------------------------------------------------------
  // 3. Open Graph Protocol (Core, Media, and Namespaces)
  // ---------------------------------------------------------------------------
  | $("meta[property='og:title'], meta[name='og:title']", "open_graph")               { @content | clean -> "title" }
  | $("meta[property='og:type'], meta[name='og:type']", "open_graph")                 { @content | clean -> "type" }
  | $("meta[property='og:url'], meta[name='og:url']", "open_graph")                   { @content | clean -> "url" }
  | $("meta[property='og:description'], meta[name='og:description']", "open_graph")   { @content | clean -> "description" }
  | $("meta[property='og:determiner'], meta[name='og:determiner']", "open_graph")     { @content | clean -> "determiner" }
  | $("meta[property='og:locale'], meta[name='og:locale']", "open_graph")             { @content | clean -> "locale" }
  | $("meta[property='og:site_name'], meta[name='og:site_name']", "open_graph")       { @content | clean -> "site_name" }
  | $("meta[property='og:updated_time'], meta[name='og:updated_time']", "open_graph") { @content | clean -> "updated_time" }
  | $("head", "open_graph") {
      find_all("meta[property='og:locale:alternate']", Value.Attribute, "content") | clean -> "locale_alternate"
    }

  | $("meta[property='og:image'], meta[property='og:image:url'], meta[name='og:image']", "open_graph.image") { @content | clean -> "url" }
  | $("meta[property='og:image:secure_url']", "open_graph.image") { @content | clean -> "secure_url" }
  | $("meta[property='og:image:type']", "open_graph.image")       { @content | clean -> "type" }
  | $("meta[property='og:image:width']", "open_graph.image")      { @content | clean -> "width" }
  | $("meta[property='og:image:height']", "open_graph.image")     { @content | clean -> "height" }
  | $("meta[property='og:image:alt']", "open_graph.image")        { @content | clean -> "alt" }

  | $("meta[property='og:video'], meta[property='og:video:url']", "open_graph.video") { @content | clean -> "url" }
  | $("meta[property='og:video:secure_url']", "open_graph.video") { @content | clean -> "secure_url" }
  | $("meta[property='og:video:type']", "open_graph.video")       { @content | clean -> "type" }
  | $("meta[property='og:video:width']", "open_graph.video")      { @content | clean -> "width" }
  | $("meta[property='og:video:height']", "open_graph.video")     { @content | clean -> "height" }

  | $("meta[property='og:audio'], meta[property='og:audio:url']", "open_graph.audio") { @content | clean -> "url" }
  | $("meta[property='og:audio:secure_url']", "open_graph.audio") { @content | clean -> "secure_url" }
  | $("meta[property='og:audio:type']", "open_graph.audio")       { @content | clean -> "type" }

  | $("meta[property='article:published_time']", "open_graph.article")  { @content | clean -> "published_time" }
  | $("meta[property='article:modified_time']", "open_graph.article")   { @content | clean -> "modified_time" }
  | $("meta[property='article:expiration_time']", "open_graph.article") { @content | clean -> "expiration_time" }
  | $("meta[property='article:author']", "open_graph.article")          { @content | clean -> "author" }
  | $("meta[property='article:section']", "open_graph.article")         { @content | clean -> "section" }
  | $("head", "open_graph.article") {
      find_all("meta[property='article:tag']", Value.Attribute, "content") | clean -> "tag"
    }

  | $("meta[property='book:author']", "open_graph.book")       { @content | clean -> "author" }
  | $("meta[property='book:isbn']", "open_graph.book")         { @content | clean -> "isbn" }
  | $("meta[property='book:release_date']", "open_graph.book") { @content | clean -> "release_date" }
  | $("meta[property='book:tag']", "open_graph.book")          { @content | clean -> "tag" }

  | $("meta[property='profile:first_name']", "open_graph.profile") { @content | clean -> "first_name" }
  | $("meta[property='profile:last_name']", "open_graph.profile")  { @content | clean -> "last_name" }
  | $("meta[property='profile:username']", "open_graph.profile")   { @content | clean -> "username" }
  | $("meta[property='profile:gender']", "open_graph.profile")     { @content | clean -> "gender" }

  | $("meta[property='music:duration']", "open_graph.music")     { @content | clean -> "duration" }
  | $("meta[property='music:album']", "open_graph.music")        { @content | clean -> "album" }
  | $("meta[property='music:album:disc']", "open_graph.music")   { @content | clean -> "album_disc" }
  | $("meta[property='music:album:track']", "open_graph.music")  { @content | clean -> "album_track" }
  | $("meta[property='music:musician']", "open_graph.music")     { @content | clean -> "musician" }
  | $("meta[property='music:song']", "open_graph.music")         { @content | clean -> "song" }
  | $("meta[property='music:release_date']", "open_graph.music") { @content | clean -> "release_date" }
  | $("meta[property='music:creator']", "open_graph.music")      { @content | clean -> "creator" }

  | $("meta[property='video:actor']", "open_graph.video_meta")        { @content | clean -> "actor" }
  | $("meta[property='video:actor:role']", "open_graph.video_meta")   { @content | clean -> "actor_role" }
  | $("meta[property='video:director']", "open_graph.video_meta")     { @content | clean -> "director" }
  | $("meta[property='video:writer']", "open_graph.video_meta")       { @content | clean -> "writer" }
  | $("meta[property='video:duration']", "open_graph.video_meta")     { @content | clean -> "duration" }
  | $("meta[property='video:release_date']", "open_graph.video_meta") { @content | clean -> "release_date" }
  | $("meta[property='video:tag']", "open_graph.video_meta")          { @content | clean -> "tag" }
  | $("meta[property='video:series']", "open_graph.video_meta")       { @content | clean -> "series" }

  | $("meta[property='product:price:amount']", "open_graph.product")     { @content | clean -> "price_amount" }
  | $("meta[property='product:price:currency']", "open_graph.product")   { @content | clean -> "price_currency" }
  | $("meta[property='product:availability']", "open_graph.product")     { @content | clean -> "availability" }
  | $("meta[property='product:brand']", "open_graph.product")            { @content | clean -> "brand" }
  | $("meta[property='product:condition']", "open_graph.product")        { @content | clean -> "condition" }
  | $("meta[property='product:retailer_item_id']", "open_graph.product") { @content | clean -> "retailer_item_id" }

  // ---------------------------------------------------------------------------
  // 4. Twitter / X Cards (incl. Slack / Discord labels & App Cards)
  // ---------------------------------------------------------------------------
  | $("meta[name='twitter:card'], meta[property='twitter:card']", "twitter")               { @content | clean -> "card" }
  | $("meta[name='twitter:site'], meta[property='twitter:site']", "twitter")               { @content | clean -> "site" }
  | $("meta[name='twitter:site:id'], meta[property='twitter:site:id']", "twitter")         { @content | clean -> "site_id" }
  | $("meta[name='twitter:creator'], meta[property='twitter:creator']", "twitter")         { @content | clean -> "creator" }
  | $("meta[name='twitter:creator:id'], meta[property='twitter:creator:id']", "twitter")   { @content | clean -> "creator_id" }
  | $("meta[name='twitter:title'], meta[property='twitter:title']", "twitter")             { @content | clean -> "title" }
  | $("meta[name='twitter:description'], meta[property='twitter:description']", "twitter") { @content | clean -> "description" }
  | $("meta[name='twitter:image'], meta[property='twitter:image'], meta[name='twitter:image:src']", "twitter") { @content | clean -> "image" }
  | $("meta[name='twitter:image:alt'], meta[property='twitter:image:alt']", "twitter")     { @content | clean -> "image_alt" }
  | $("meta[name='twitter:player'], meta[property='twitter:player']", "twitter")           { @content | clean -> "player" }
  | $("meta[name='twitter:player:width'], meta[property='twitter:player:width']", "twitter")   { @content | clean -> "player_width" }
  | $("meta[name='twitter:player:height'], meta[property='twitter:player:height']", "twitter") { @content | clean -> "player_height" }
  | $("meta[name='twitter:player:stream'], meta[property='twitter:player:stream']", "twitter") { @content | clean -> "player_stream" }
  | $("meta[name='twitter:label1']", "twitter") { @content | clean -> "label1" }
  | $("meta[name='twitter:data1']", "twitter")  { @content | clean -> "data1" }
  | $("meta[name='twitter:label2']", "twitter") { @content | clean -> "label2" }
  | $("meta[name='twitter:data2']", "twitter")  { @content | clean -> "data2" }
  | $("meta[name='twitter:app:name:iphone']", "twitter.app.iphone")     { @content | clean -> "name" }
  | $("meta[name='twitter:app:id:iphone']", "twitter.app.iphone")       { @content | clean -> "id" }
  | $("meta[name='twitter:app:url:iphone']", "twitter.app.iphone")      { @content | clean -> "url" }
  | $("meta[name='twitter:app:name:ipad']", "twitter.app.ipad")         { @content | clean -> "name" }
  | $("meta[name='twitter:app:id:ipad']", "twitter.app.ipad")           { @content | clean -> "id" }
  | $("meta[name='twitter:app:url:ipad']", "twitter.app.ipad")          { @content | clean -> "url" }
  | $("meta[name='twitter:app:name:googleplay']", "twitter.app.googleplay") { @content | clean -> "name" }
  | $("meta[name='twitter:app:id:googleplay']", "twitter.app.googleplay")   { @content | clean -> "id" }
  | $("meta[name='twitter:app:url:googleplay']", "twitter.app.googleplay")  { @content | clean -> "url" }

  // ---------------------------------------------------------------------------
  // 5. Facebook (fb:*, ia:*, domain)
  // ---------------------------------------------------------------------------
  | $("meta[property='fb:app_id']", "facebook")         { @content | clean -> "app_id" }
  | $("meta[property='fb:admins']", "facebook")         { @content | clean -> "admins" }
  | $("meta[property='fb:pages']", "facebook")          { @content | clean -> "pages" }
  | $("meta[property='fb:profile_id']", "facebook")     { @content | clean -> "profile_id" }
  | $("meta[property='ia:markup_url']", "facebook")     { @content | clean -> "ia_markup_url" }
  | $("meta[property='ia:markup_url_dev']", "facebook") { @content | clean -> "ia_markup_url_dev" }
  | $("meta[property='ia:rules_url']", "facebook")      { @content | clean -> "ia_rules_url" }
  | $("meta[property='ia:rules_url_dev']", "facebook")  { @content | clean -> "ia_rules_url_dev" }
  | $("meta[name='facebook-domain-verification']", "facebook") { @content | clean -> "domain_verification" }

  // ---------------------------------------------------------------------------
  // 6. Instagram & App Links (instapp:*, al:*)
  // ---------------------------------------------------------------------------
  | $("meta[property='instapp:owner_user_id']", "instagram_and_applinks") { @content | clean -> "instapp_owner_user_id" }
  | $("meta[property='instapp:hashtags']", "instagram_and_applinks")      { @content | clean -> "instapp_hashtags" }
  | $("meta[property='al:ios:url']", "instagram_and_applinks.al_ios")            { @content | clean -> "url" }
  | $("meta[property='al:ios:app_store_id']", "instagram_and_applinks.al_ios")   { @content | clean -> "app_store_id" }
  | $("meta[property='al:ios:app_name']", "instagram_and_applinks.al_ios")       { @content | clean -> "app_name" }
  | $("meta[property='al:android:url']", "instagram_and_applinks.al_android")        { @content | clean -> "url" }
  | $("meta[property='al:android:package']", "instagram_and_applinks.al_android")    { @content | clean -> "package" }
  | $("meta[property='al:android:app_name']", "instagram_and_applinks.al_android")   { @content | clean -> "app_name" }
  | $("meta[property='al:web:url']", "instagram_and_applinks.al_web")            { @content | clean -> "url" }
  | $("meta[property='al:web:should_fallback']", "instagram_and_applinks.al_web"){ @content | clean -> "should_fallback" }

  // ---------------------------------------------------------------------------
  // 7. Dublin Core (DC.* and DCTERMS.*)
  // ---------------------------------------------------------------------------
  | $("meta[name='DC.title'], meta[name='dc.title']", "dublin_core")             { @content | clean -> "dc_title" }
  | $("meta[name='DC.creator'], meta[name='dc.creator']", "dublin_core")         { @content | clean -> "dc_creator" }
  | $("meta[name='DC.subject'], meta[name='dc.subject']", "dublin_core")         { @content | clean -> "dc_subject" }
  | $("meta[name='DC.description'], meta[name='dc.description']", "dublin_core") { @content | clean -> "dc_description" }
  | $("meta[name='DC.publisher'], meta[name='dc.publisher']", "dublin_core")     { @content | clean -> "dc_publisher" }
  | $("meta[name='DC.contributor'], meta[name='dc.contributor']", "dublin_core") { @content | clean -> "dc_contributor" }
  | $("meta[name='DC.date'], meta[name='dc.date']", "dublin_core")               { @content | clean -> "dc_date" }
  | $("meta[name='DC.type'], meta[name='dc.type']", "dublin_core")               { @content | clean -> "dc_type" }
  | $("meta[name='DC.format'], meta[name='dc.format']", "dublin_core")           { @content | clean -> "dc_format" }
  | $("meta[name='DC.identifier'], meta[name='dc.identifier']", "dublin_core")   { @content | clean -> "dc_identifier" }
  | $("meta[name='DC.source'], meta[name='dc.source']", "dublin_core")           { @content | clean -> "dc_source" }
  | $("meta[name='DC.language'], meta[name='dc.language']", "dublin_core")       { @content | clean -> "dc_language" }
  | $("meta[name='DC.relation'], meta[name='dc.relation']", "dublin_core")       { @content | clean -> "dc_relation" }
  | $("meta[name='DC.coverage'], meta[name='dc.coverage']", "dublin_core")       { @content | clean -> "dc_coverage" }
  | $("meta[name='DC.rights'], meta[name='dc.rights']", "dublin_core")           { @content | clean -> "dc_rights" }
  | $("meta[name='DCTERMS.created'], meta[name='dcterms.created']", "dublin_core")   { @content | clean -> "dcterms_created" }
  | $("meta[name='DCTERMS.modified'], meta[name='dcterms.modified']", "dublin_core") { @content | clean -> "dcterms_modified" }
  | $("meta[name='DCTERMS.issued'], meta[name='dcterms.issued']", "dublin_core")     { @content | clean -> "dcterms_issued" }
  | $("meta[name='DCTERMS.abstract'], meta[name='dcterms.abstract']", "dublin_core") { @content | clean -> "dcterms_abstract" }

  // ---------------------------------------------------------------------------
  // 8. Academic & Publishing (Google Scholar, Parse.ly, Sailthru)
  // ---------------------------------------------------------------------------
  | $("meta[name='citation_title']", "academic_and_publishing")            { @content | clean -> "citation_title" }
  | $("head", "academic_and_publishing") {
      find_all("meta[name='citation_author']", Value.Attribute, "content") | clean -> "citation_author"
    }
  | $("meta[name='citation_publication_date']", "academic_and_publishing") { @content | clean -> "citation_publication_date" }
  | $("meta[name='citation_journal_title']", "academic_and_publishing")    { @content | clean -> "citation_journal_title" }
  | $("meta[name='citation_volume']", "academic_and_publishing")           { @content | clean -> "citation_volume" }
  | $("meta[name='citation_issue']", "academic_and_publishing")            { @content | clean -> "citation_issue" }
  | $("meta[name='citation_firstpage']", "academic_and_publishing")        { @content | clean -> "citation_firstpage" }
  | $("meta[name='citation_lastpage']", "academic_and_publishing")         { @content | clean -> "citation_lastpage" }
  | $("meta[name='citation_doi']", "academic_and_publishing")              { @content | clean -> "citation_doi" }
  | $("meta[name='citation_pmid']", "academic_and_publishing")             { @content | clean -> "citation_pmid" }
  | $("meta[name='citation_arxiv_id']", "academic_and_publishing")         { @content | clean -> "citation_arxiv_id" }
  | $("meta[name='citation_issn']", "academic_and_publishing")             { @content | clean -> "citation_issn" }
  | $("meta[name='citation_isbn']", "academic_and_publishing")             { @content | clean -> "citation_isbn" }
  | $("meta[name='citation_pdf_url']", "academic_and_publishing")          { @content | clean -> "citation_pdf_url" }
  | $("meta[name='citation_language']", "academic_and_publishing")         { @content | clean -> "citation_language" }
  | $("meta[name='parsely-title']", "academic_and_publishing")    { @content | clean -> "parsely_title" }
  | $("meta[name='parsely-link']", "academic_and_publishing")     { @content | clean -> "parsely_link" }
  | $("meta[name='parsely-type']", "academic_and_publishing")     { @content | clean -> "parsely_type" }
  | $("meta[name='parsely-pub-date']", "academic_and_publishing") { @content | clean -> "parsely_pub_date" }
  | $("meta[name='parsely-section']", "academic_and_publishing")  { @content | clean -> "parsely_section" }
  | $("meta[name='parsely-author']", "academic_and_publishing")   { @content | clean -> "parsely_author" }
  | $("meta[name='parsely-tags']", "academic_and_publishing")     { @content | clean -> "parsely_tags" }
  | $("meta[name='parsely-metadata']", "academic_and_publishing") { @content | clean -> "parsely_metadata" }
  | $("meta[name='sailthru.title']", "academic_and_publishing")      { @content | clean -> "sailthru_title" }
  | $("meta[name='sailthru.tags']", "academic_and_publishing")       { @content | clean -> "sailthru_tags" }
  | $("meta[name='sailthru.date']", "academic_and_publishing")       { @content | clean -> "sailthru_date" }
  | $("meta[name='sailthru.author']", "academic_and_publishing")     { @content | clean -> "sailthru_author" }
  | $("meta[name='sailthru.image.full']", "academic_and_publishing") { @content | clean -> "sailthru_image_full" }

  // ---------------------------------------------------------------------------
  // 9. Apple, Microsoft, Pinterest & Domain Verification
  // ---------------------------------------------------------------------------
  | $("meta[name='apple-mobile-web-app-capable']", "apple")          { @content | clean -> "mobile_web_app_capable" }
  | $("meta[name='apple-mobile-web-app-status-bar-style']", "apple") { @content | clean -> "mobile_web_app_status_bar_style" }
  | $("meta[name='apple-mobile-web-app-title']", "apple")            { @content | clean -> "mobile_web_app_title" }
  | $("meta[name='apple-itunes-app']", "apple")                      { @content | clean -> "itunes_app" }
  | $("meta[name='apple-touch-fullscreen']", "apple")                { @content | clean -> "touch_fullscreen" }

  | $("meta[name='msapplication-TileColor']", "microsoft")       { @content | clean -> "tile_color" }
  | $("meta[name='msapplication-TileImage']", "microsoft")       { @content | clean -> "tile_image" }
  | $("meta[name='msapplication-tooltip']", "microsoft")         { @content | clean -> "tooltip" }
  | $("meta[name='msapplication-config']", "microsoft")          { @content | clean -> "config" }
  | $("meta[name='msapplication-navbutton-color']", "microsoft") { @content | clean -> "navbutton_color" }
  | $("meta[name='msapplication-tap-highlight']", "microsoft")   { @content | clean -> "tap_highlight" }

  | $("meta[name='pinterest']", "pinterest")          { @content | clean -> "directive" }
  | $("meta[name='pinterest-rich-pin']", "pinterest") { @content | clean -> "rich_pin" }

  | $("meta[name='google-site-verification']", "verification")         { @content | clean -> "google" }
  | $("meta[name='msvalidate.01']", "verification")                    { @content | clean -> "bing" }
  | $("meta[name='yandex-verification']", "verification")              { @content | clean -> "yandex" }
  | $("meta[name='baidu-site-verification']", "verification")          { @content | clean -> "baidu" }
  | $("meta[name='p:domain_verify']", "verification")                  { @content | clean -> "pinterest" }
  | $("meta[name='norton-safeweb-site-verification']", "verification") { @content | clean -> "norton" }
  | $("meta[name='ahrefs-site-verification']", "verification")         { @content | clean -> "ahrefs" }

  // ---------------------------------------------------------------------------
  // 10. <link> Relations (SEO, Icons, Feeds, IndieWeb, Fediverse, oEmbed)
  // ---------------------------------------------------------------------------
  | $("link[rel='canonical']", "links")        { @href | clean -> "canonical" }
  | $("link[rel='amphtml']", "links")          { @href | clean -> "amphtml" }
  | $("link[rel='manifest']", "links")         { @href | clean -> "manifest" }
  | $("link[rel='prev']", "links")             { @href | clean -> "prev" }
  | $("link[rel='next']", "links")             { @href | clean -> "next" }
  | $("link[rel='author']", "links")           { @href | clean -> "author" }
  | $("link[rel='license']", "links")          { @href | clean -> "license" }
  | $("link[rel='icon'], link[rel='shortcut icon']", "links") { @href | clean -> "icon" }
  | $("link[rel='apple-touch-icon'], link[rel='apple-touch-icon-precomposed']", "links") { @href | clean -> "apple_touch_icon" }
  | $("link[rel='mask-icon']", "links")        { @href | clean -> "mask_icon" }
  | $("link[rel='me']", "links")               { @href | clean -> "me" }
  | $("link[rel='webmention']", "links")       { @href | clean -> "webmention" }
  | $("link[rel='micropub']", "links")         { @href | clean -> "micropub" }
  | $("link[rel='microsub']", "links")         { @href | clean -> "microsub" }
  | $("link[rel='pingback']", "links")         { @href | clean -> "pingback" }
  | $("link[rel='hub']", "links")              { @href | clean -> "hub" }
  | $("link[rel='EditURI']", "links")          { @href | clean -> "edit_uri" }
  | $("link[rel='search']", "links")           { @href | clean -> "search" }
  | $("link[rel='authorization_endpoint']", "links") { @href | clean -> "authorization_endpoint" }
  | $("link[rel='token_endpoint']", "links")         { @href | clean -> "token_endpoint" }
  | $("link[rel='monetization']", "links")           { @href | clean -> "monetization" }
  | $("link[rel='alternate'][type='application/rss+xml']", "links")          { @href | clean -> "rss" }
  | $("link[rel='alternate'][type='application/atom+xml']", "links")         { @href | clean -> "atom" }
  | $("link[rel='alternate'][type='application/activity+json']", "links")    { @href | clean -> "activitypub" }
  | $("link[rel='alternate'][type='application/json+oembed']", "links")      { @href | clean -> "oembed_json" }
  | $("link[rel='alternate'][type='text/xml+oembed']", "links")              { @href | clean -> "oembed_xml" }
  | $("link[rel='alternate']", "links.alternates") {
      @href     | clean -> "href"
      | @hreflang | clean -> "hreflang"
      | @type     | clean -> "type"
      | @title    | clean -> "title"
    }

  // ---------------------------------------------------------------------------
  // 11. JSON-LD (Schema.org) & Embedded JSONs / States
  // ---------------------------------------------------------------------------
  | $("script[type='application/ld+json']", "json_ld") {
      text | json_parse(strict=false)
    }
  | $("script#__NEXT_DATA__", "embedded_json.next_data") {
      text | json_parse(strict=false)
    }
  | $("script#__NUXT_DATA__", "embedded_json.nuxt_data") {
      text | json_parse(strict=false)
    }
  | $("script#ng-state", "embedded_json.angular_state") {
      text | json_parse(strict=false)
    }
  | $("script[data-sveltekit-fetched]", "embedded_json.sveltekit_fetched") {
      text | json_parse(strict=false)
    }
  | $("script[type='importmap']", "embedded_json.importmap") {
      text | json_parse(strict=false)
    }
  | $("script[type='speculationrules']", "embedded_json.speculationrules") {
      text | json_parse(strict=false)
    }
  | $("script[type='application/json']", "embedded_json.json_scripts") {
      @id  | clean -> "id"
      | text | json_parse("data", strict=false)
    }

  // Inline JavaScript States (Nuxt, Remix, Redux/Initial State, Apollo, Shopify)
  | $("script", "embedded_json") {
      text | js_block("window.__NUXT__ =") | parse_js("nuxt_window", true)
      | text | js_block("window.__remixContext =") | parse_js("remix_context", true)
      | text | js_block("window.__INITIAL_STATE__ =") | parse_js("initial_state", true)
      | text | js_block("window.__PRELOADED_STATE__ =") | parse_js("preloaded_state", true)
      | text | js_block("window.__APOLLO_STATE__ =") | parse_js("apollo_state", true)
      | text | js_block("window.ShopifyAnalytics =") | parse_js("shopify_analytics", true)
    }

  // ---------------------------------------------------------------------------
  // 12. Microdata, RDFa & Definition Lists (<dl>, <dt>, <dd>)
  // ---------------------------------------------------------------------------
  | $("[itemscope]", "microdata.scopes") {
      @itemtype   | clean -> "itemtype"
      | @itemid   | clean -> "itemid"
      | @itemprop | clean -> "itemprop"
    }
  | $("[itemprop]", "microdata.props") {
      @itemprop   | clean -> "itemprop"
      | @content  | clean -> "content"
      | @href     | clean -> "href"
      | @src      | clean -> "src"
      | @datetime | clean -> "datetime"
      | @value    | clean -> "value"
      | text      | clean -> "text"
    }
  | $("[typeof]", "rdfa.types") {
      @typeof   | clean -> "typeof"
      | @vocab    | clean -> "vocab"
      | @resource | clean -> "resource"
    }
  | $("body [property]", "rdfa.properties") {
      @property | clean -> "property"
      | @content | clean -> "content"
      | @href    | clean -> "href"
      | text     | clean -> "text"
    }
  | $("dl", "definition_lists.lists") {
      @id         | clean -> "id"
      | @class    | clean -> "class"
      | @itemprop | clean -> "itemprop"
    }
  | $("dl dt", "definition_lists.terms") {
      text        | clean -> "text"
      | @itemprop | clean -> "itemprop"
    }
  | $("dl dd", "definition_lists.descriptions") {
      text        | clean -> "text"
      | @itemprop | clean -> "itemprop"
      | @content  | clean -> "content"
      | @href     | clean -> "href"
    }

  // ---------------------------------------------------------------------------
  // 13. Generic Fallback (all <meta> and <link> tags in the document)
  // ---------------------------------------------------------------------------
  | $("head meta", "all_meta") {
      @name           | clean -> "name"
      | @property     | clean -> "property"
      | @"http-equiv" | clean -> "http_equiv"
      | @charset      | clean -> "charset"
      | @content      | clean -> "content"
    }
  | $("head link", "all_links") {
      @rel      | clean -> "rel"
      | @href     | clean -> "href"
      | @type     | clean -> "type"
      | @hreflang | clean -> "hreflang"
      | @sizes    | clean -> "sizes"
      | @title    | clean -> "title"
    }


// =============================================================================
// Scraper 2: Normalized Summary with Fallbacks (via JMESPath)
// =============================================================================
metadata_summary = Scraper()
  | $("html") {
      @lang | clean -> "lang"
      | find("head title", Value.Text) | clean -> "basic_title"
      | find("meta[name='description']", Value.Attribute, "content") | clean -> "basic_description"
      | find("meta[name='author']", Value.Attribute, "content") | clean -> "basic_author"
      | find("link[rel='canonical']", Value.Attribute, "href") | clean -> "canonical"
      | find("meta[property='og:title']", Value.Attribute, "content") | clean -> "og_title"
      | find("meta[property='og:description']", Value.Attribute, "content") | clean -> "og_description"
      | find("meta[property='og:url']", Value.Attribute, "content") | clean -> "og_url"
      | find("meta[property='og:image']", Value.Attribute, "content") | clean -> "og_image"
      | find("meta[property='og:type']", Value.Attribute, "content") | clean -> "og_type"
      | find("meta[property='og:site_name']", Value.Attribute, "content") | clean -> "og_site_name"
      | find("meta[property='article:published_time']", Value.Attribute, "content") | clean -> "published_time"
      | find("meta[name='twitter:title']", Value.Attribute, "content") | clean -> "tw_title"
      | find("meta[name='twitter:description']", Value.Attribute, "content") | clean -> "tw_description"
      | find("meta[name='twitter:image']", Value.Attribute, "content") | clean -> "tw_image"
    }
  | $("script[type='application/ld+json']", "json_ld") { text | json_parse(strict=false) }
  | jmespath("{
      title: not_null(og_title, tw_title, basic_title),
      description: not_null(og_description, tw_description, basic_description),
      url: not_null(canonical, og_url),
      image: not_null(og_image, tw_image),
      type: og_type,
      site_name: og_site_name,
      author: basic_author,
      published_time: published_time,
      lang: lang,
      json_ld: json_ld
    }
  ")
