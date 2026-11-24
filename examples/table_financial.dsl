// =============================================================================
// Financial Table & Matrix Extraction Pipeline
// =============================================================================

market_summary = Scraper()
  | $("header") {
      find("h1") | clean -> "index_title"
      | find("span.timestamp", Attribute, "data-epoch") | to_int(key="epoch")
    }
  | $("table.financial-matrix") {
      table(key="stocks", header=true, normalize_headers=true, trim_cells=true)
    }
  | jmespath("{
      index: index_title,
      updated_epoch: epoch,
      buy_recommendations: stocks[?rating == 'Strong Buy' || rating == 'Buy'].{
        symbol: ticker,
        company: company,
        price: price,
        change: change,
        market_cap: market_cap
      }
    }
  ")
