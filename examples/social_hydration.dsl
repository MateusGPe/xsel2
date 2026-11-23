// =============================================================================
// Social Media Post & JavaScript Hydration Extraction Pipeline
// =============================================================================

social = Scraper()
  .$(".post-card") {
    find(".author-row .handle") | clean -> "author_handle"
    | find(".content") | clean -> "post_text"
    | find(".engagement .likes", Attribute, "data-count") | to_number(key="likes_count")
    | find(".engagement .reposts", Attribute, "data-count") | to_number(key="reposts_count")
  }
  .$("script") {
    text | js_block("window.__INITIAL_DATA__ =") | parse_js("payload", true)
  }
  | jmespath("{author: payload.author.username, followers: payload.author.followers, post: post_text, likes: likes_count, top_comments: payload.comments[?score > `50`].{user: user, score: score, text: text}}")
