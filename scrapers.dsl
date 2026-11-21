# xsel2 DSL version of data/scrapers.dsl
# Refactored with single-pass fields(), map_values(), js_blocks() aliases,
# tee { ... }, query_params(), pipe/use, coalesce_keys(), and finalize { ... }
# while preserving exact output schemas and selectors.

let SCRAPER_VARS_NAMES = [
    "ADD_TO_MODAL", "MODEL_PROFILE", "SUGGEST_PORNSTAR_BOX", "VIDEO_SHOW",
    "WIDGET_BUTTON_FRIEND", "WIDGET_BUTTON_SUBSCRIBE", "WIDGET_RATINGS_LIKE_FAV",
    "WIDGET_SHARE", "nextVideoPlaylistObject", "playerObjList",
    "proposePornstarUrl", "varObj_footer", "dataLayer", "relatedVideosData", "adRollGlobalConfig"
];

let IBOX_RANK_MAP = {
    "monthly rank": "ibox_rank_monthly",
    "weekly rank": "ibox_rank_weekly",
    "yearly rank": "ibox_rank_yearly",
    "last month": "ibox_rank_last_month",
    "model rank": "ibox_rank_model",
    "subscribers": "ibox_subscribers",
    "video views": "ibox_video_views"
};

let INFO_PIECE_MAP = {
    "gender": "gender_name",
    "interested_in": "interest_type",
    "interests_and_hobbies": "ints_hobbies",
    "interests_and_hobbie": "ints_hobbies",
    "relationship_status": "relship_status",
    "career_start_and_end": "career_range"
};

pipe link_map = {
    text | squish -> "@key"
    | @href | squish -> "@value"
};

// ============================================================================
// 1. SharedPageData
// ============================================================================
shared_page_data := Scraper()
    .select(".statsUser .stat-item", "hover_userinfo_items") {
        find_first(".label", Value.Text, squish=false) | trim | lower -> "label_text"
        | find_first(".count", Value.Text, squish=false) | trim -> "count_text"
    }
    .select(".socialList li a", "social_list") { use(link_map) }
    .select("html") {
        // Subscribe ID
        find_first(".subscribeButton, .subscribeButton button, [data-button-id^=\"subscribe_\"]", Value.Attribute, "data-button-id", squish=false)
            | replace("subscribe_", "") | squish -> "subscribe_id"
        | find_first(".subscribeButton button[data-id], .subscribeButton[data-id]", Value.Attribute, "data-id") -> "subscribe_id"

        // Single-pass DOM & metadata fields
        | fields({
            // User Meta (Common)
            meta_is_channel_type: { sel: "#channelsProfile, #channelsBody, .sectionChannelsWrapper", type: exists },
            meta_is_user_type: { sel: ".profileUserName, .amateurModel, .profileClass, #avatarPicture, .topProfileHeader, .viewProfileContainer", type: exists },

            // Page Scripts Flags
            script_page_video_removed: { sel: ".removed", type: exists },
            script_page_mpp_geo_blocked: { sel: ".geoBlocked", type: exists },
            script_page_video_notice: { sel: ".video-notice span, .video-notice", type: exists },
            script_page_video_not_found: { sel: ".mainPlayerDiv .noVideo, .noVideo", type: exists },
            script_page_video_page: { sel: ".mainPlayerDiv, #player, #resume_button", type: exists },
            script_page_video_flagged: { sel: ".trustAndSafetyFlagWrapper", type: exists },

            // Hover Data
            hover_cover: { sel: "img.avatarCover[src], img.avatarCover[data-src]", attr: ["src", "data-src"], squish: false },
            hover_thumb: { sel: "img.avatarIcon[src], img.avatarIcon[data-src]", attr: ["src", "data-src"], squish: false },
            hover_premium_exists: { sel: ".premium-icon", type: exists },
            hover_verified_exists: { sel: ".verified-icon", type: exists },
            hover_award_exists: { sel: ".award-icon", type: exists },
            hover_username_text: { sel: "a.username", squish: false, trim: true },
            hover_username_href: { sel: "a.username", attr: "href", squish: false },

            // Video Config
            config_inline_title: "h1.title .inlineFree, h1.title span, h1.title, title",
            config_view_count: ".views .count, .ratingInfo .views .count",
            config_video_info: ".videoInfo, .ratingInfo .videoInfo",
            config_votes_up: ".votes-fav-wrap .votesUp, .votesUp",
            config_user_name: ".userInfo .usernameWrap .bolded, .userInfo .usernameBadgesWrapper .bolded, .userInfo .usernameWrap a, .video-info-row .userInfo a",
            config_user_link: { sel: ".userInfo .usernameWrap .bolded, .userInfo .usernameBadgesWrapper .bolded, .userInfo .usernameWrap a, .video-info-row .userInfo a", attr: "href" }
        })
    };

// ============================================================================
// 2. UserProfile
// ============================================================================
user_profile := Scraper()
    .select(".infoBox", "iboxes") {
        find_first(".big, .number") | lower -> "@value"
        | find_first(".title") | lower | map_values(IBOX_RANK_MAP) -> "@key"
    }
    .select(".model-details .infoPiece", "info_pieces") {
        find_first("span:first-of-type")
            | replace_many({":": "", " ": "_"})
            | lower
            | map_values(INFO_PIECE_MAP)
            | prepend("info_") -> "@key"
        | find_first("span.smallInfo") -> "@value"
    }
    .select("dt", "profile_info") {
        text | squish | replace_many({":": "", "\"": "", " ": "_"}) | lower -> "@key"
        | find_first("dd", Value.Adjacent, squish=false) | replace_exact("\"", "") | squish -> "@value"
    }
    .select(".featuredIn a", "featured_in") { use(link_map) }
    .select("html") {
        fields({
            meta_username: "h1[itemprop=\"name\"], .profileUserName a[title], #channelsProfile h1",
            actor_id: { sel: "input[name=\"pageUser\"]", attr: "value" },
            meta_about_me: { sel: ".aboutMeSection :not(.title), .aboutMeText, .aboutMeSection .text", first: false, join: " " },
            about_me_cdesc: ".cdescriptions:first-child",
            meta_bio: ".biographyText [itemprop=description], .biographyText",
            long_bio: ".bio .longBio",
            meta_modelprofilelink: { sel: "#profileHome a, .profileUserName a[title], #channelsProfile .subFilterList .active a", attr: "href" },
            meta_avatar: { sel: "#getAvatar[src], #avatarPicture img[src], .avatarPicture img[src]", attr: "src" },
            friend_id: { sel: ".addFriendButton button[data-id], .addFriendButton[data-id]", attr: "data-id" },
            meta_tips: { sel: ".tooltipTrig.infoBox[data-title], .infoBox[data-title]", attr: "data-title", first: false, squish: false },

            // Video counts & subscribers
            videos: ".subHeaderOverrite .showingInfo .totalSpan, .showingInfo .totalSpan",
            uploaded_videos_label: ".subHeaderOverrite h2 a[href*=\"upload\"]",
            public_videos_label: ".subHeaderOverrite h2 a[href*=\"public\"]",
            subscribers_count: ".subscribersCount, .userSubscribers .count",

            // Badges
            meta_user_trophy: { sel: ".badge-username .trophyPornStar, .trophyPornStar", type: exists },
            meta_verified_status: { sel: ".badge-username .verifiedPornstar, .verifiedPornstar", type: exists },

            // Redundant user/channel meta preserved
            user_un: ".profileUserName a[title], .profileUserName a",
            user_mpl: { sel: ".profileUserName a[title], .profileUserName a", attr: "href", squish: false },
            channel_un: "#channelsProfile h1",
            channel_mpl: { sel: "#channelsProfile .subFilterList .active a", attr: "href", squish: false },

            // Headers joined directly in DSL
            headers_showing_info: { sel: ".subHeaderOverrite .showingInfo", first: false, join: ", " }
        })
        | try_find(".info", Value.Text) | squish | upper -> "infos"
        | try_find(".info.floatRight", Value.Text) | squish | lower -> "headers_alt"
        | try_find(".subHeaderOverrite :nth-child(1)", Value.Text) | squish | lower | join(", ") -> "headers_first"
    }
    .select(".subHeaderOverrite", "subheader_sections") {
        find_first("h2 a", Value.Attribute, "href") -> "href"
        | find_first(".showingInfo .totalSpan") -> "total"
    }
    .finalize {
        jmespath('merge(@, not_null(iboxes, `{}`), not_null(info_pieces, `{}`), {
            uploaded_videos: (subheader_sections[?contains(not_null(href, \'\'), \'upload\')].total | [0]),
            public_videos: (subheader_sections[?contains(not_null(href, \'\'), \'public\')].total | [0]),
            premium_videos: (subheader_sections[?contains(not_null(href, \'\'), \'premium\')].total | [0])
        })')
        | omit_keys(["iboxes", "info_pieces", "subheader_sections"])
    };

// ============================================================================
// 3. VideoPage
// ============================================================================
video_page := Scraper()
    .select("html") {
        // 3a+3b. Single-pass JS blocks including dynamic flashvars_<id>
        find_all("script", Value.Text, optional=true) | strip_comments | squish | js_blocks([
            "MODEL_PROFILE",
            "WIDGET_RATINGS_LIKE_FAV",
            "VIDEO_SHOW",
            "WIDGET_SHARE",
            "ADD_TO_MODAL",
            "SUGGEST_PORNSTAR_BOX",
            "straightLangUrl",
            "relatedVideosData",
            "flashvars_*"
        ])
        // 3c. JSON-LD VideoObject + meta tags via extract_meta
        | extract_meta(key="ld", include_json_ld=true)
        // 3d. Single-pass DOM & meta fallbacks
        | fields({
            config_i_meta_title_alt: { sel: "meta[name=\"twitter:title\"], meta[property=\"og:title\"]", attr: "content" },
            config_i_meta_duration_alt: { sel: "meta[property=\"video:duration\"]", attr: "content" },
            config_i_meta_title: { sel: "meta[name=\"twitter:title\"], meta[property=\"og:title\"]", attr: "content", squish: false },
            config_i_meta_duration: { sel: "meta[property=\"video:duration\"]", attr: "content", squish: false },
            scripts_page_link_canon: { sel: "[rel=\"canonical\"], [rel=\"next\"]", attr: "href", squish: false },
            i_rating_percent: ".ratingPercent .percent, .video-rating .percent",
            i_votes_down: ".votes-fav-wrap .votesDown",
            i_favorites_counter: ".favorite-wrapper .favoritesCounter",
            dom_votes_up: { sel: ".votes-fav-wrap .votesUp", attr: "data-rating" },
            user_avatar: { sel: ".userInfoBlock .userAvatar img[src]", attr: "src" },
            dom_user_id: { sel: ".userInfo .usernameWrap[data-userid], .userInfo .usernameWrap[data-channelid]", attr: "data-userid" },
            dom_channel_id: { sel: ".userInfo .usernameWrap[data-channelid]", attr: "data-channelid" },
            user_type: { sel: ".userInfo .usernameWrap[data-type]", attr: "data-type" },
            dom_video_info: ".ratingInfo .videoInfo, .videoInfo"
        })
        | try_find(".pornstarsWrapper a.pstar-list-btn", Value.Text) | squish | drop_empty | unique -> "dom_pornstars"
    }
    .finalize {
        jmespath('{
            video_id: not_null(VIDEO_SHOW.vkey, MODEL_PROFILE.viewkey, WIDGET_RATINGS_LIKE_FAV.vkey, VIDEO_SHOW.video_id, flashvars.playbackTracking.video_id),
            scripts_viewkey: not_null(VIDEO_SHOW.vkey, MODEL_PROFILE.viewkey, WIDGET_RATINGS_LIKE_FAV.vkey, VIDEO_SHOW.video_id, flashvars.playbackTracking.video_id),
            scripts_segment: VIDEO_SHOW.segment,
            scripts_vr_enabled: VIDEO_SHOW.vr,
            scripts_watch_later_random_url: VIDEO_SHOW.watchLaterRandomUrl,
            scripts_fake_random_url: VIDEO_SHOW.fakeRandomUrl,
            scripts_fake_videos_url: VIDEO_SHOW.fakeVideosUrl,
            scripts_model_profile_link: MODEL_PROFILE.modelProfileLink,
            scripts_username: MODEL_PROFILE.username,
            scripts_is_vr: not_null(flashvars.isVR, VIDEO_SHOW.vr),
            scripts_video_unavailable: flashvars.video_unavailable,
            scripts_is_hd: flashvars.isHD,
            scripts_video_unavailable_country: flashvars.video_unavailable_country,
            scripts_action_tags: flashvars.actionTags,
            scripts_video_title_original: VIDEO_SHOW.videoTitleOriginal,
            scripts_video_title_translated: VIDEO_SHOW.videoTitleTranslated,
            scripts_video_title: not_null(VIDEO_SHOW.videoTitle, flashvars.video_title, config_i_meta_title),
            scripts_video_title_alt: not_null(flashvars.video_title, VIDEO_SHOW.videoTitle),
            scripts_current_up: not_null(WIDGET_RATINGS_LIKE_FAV.currentUp, dom_votes_up),
            scripts_current_down: WIDGET_RATINGS_LIKE_FAV.currentDown,
            scripts_item_id_num: WIDGET_RATINGS_LIKE_FAV.itemIdNum,
            scripts_is_premium: not_null(VIDEO_SHOW.isPremium, WIDGET_RATINGS_LIKE_FAV.isPremium, MODEL_PROFILE.isPremium),
            scripts_geo: flashvars.geo,
            scripts_embed_id: flashvars.embedId,
            scripts_embed_code: flashvars.embedCode,
            scripts_tag_suggestion_url: VIDEO_SHOW.tag_suggestion_url,
            scripts_category_suggestion_url: VIDEO_SHOW.categorySuggestionUrl,
            scripts_track_video_id: VIDEO_SHOW.trackVideoId,
            scripts_video_id_camel: not_null(VIDEO_SHOW.videoId, WIDGET_RATINGS_LIKE_FAV.itemId),
            scripts_language: flashvars.language,
            scripts_isp: flashvars.isp,
            scripts_placeholder: not_null(VIDEO_SHOW.placeholder, ADD_TO_MODAL.videoThumb),
            scripts_image_url: not_null(flashvars.image_url, VIDEO_SHOW.placeholder, ADD_TO_MODAL.videoThumb, (ld.json_ld[?"@type" == \'VideoObject\'].thumbnailUrl | [0])),
            scripts_traffic_junky_url: VIDEO_SHOW.trafficJunkyurl,
            scripts_related_url: flashvars.related_url,
            scripts_url_share: WIDGET_SHARE.urlShare,
            scripts_share_title: WIDGET_SHARE.shareTitle,
            scripts_is_vertical: flashvars.isVertical,
            scripts_link_url: not_null(flashvars.link_url, ADD_TO_MODAL.videoUrl),
            scripts_video_duration: not_null(flashvars.video_duration, flashvars.playbackTracking.video_duration, config_i_meta_duration),
            scripts_video_timestamp: flashvars.playbackTracking.video_timestamp,
            scripts_media_priority: flashvars.mediaPriority,
            scripts_default_quality: flashvars.defaultQuality,
            scripts_hotspots: flashvars.hotspots,
            scripts_media_definitions: flashvars.mediaDefinitions,
            scripts_json: flashvars.json,
            videos: relatedVideosData.relatedVideos.fields,
            scripts_sampling_frequency: flashvars.thumbs.samplingFrequency,
            scripts_thumb_width: flashvars.thumbs.thumbWidth,
            scripts_thumb_height: flashvars.thumbs.thumbHeight,
            scripts_type: flashvars.thumbs.type,
            scripts_url_pattern: flashvars.thumbs.urlPattern,
            spritePatterns: flashvars.thumbs.spritePatterns,
            scripts_s_name: (ld.json_ld[?"@type" == \'VideoObject\'].name | [0]),
            scripts_s_duration: (ld.json_ld[?"@type" == \'VideoObject\'].duration | [0]),
            scripts_s_upload_date: (ld.json_ld[?"@type" == \'VideoObject\'].uploadDate | [0]),
            scripts_s_thumbnail_url: (ld.json_ld[?"@type" == \'VideoObject\'].thumbnailUrl | [0]),
            scripts_s_description: (ld.json_ld[?"@type" == \'VideoObject\'].description | [0]),
            scripts_s_author: not_null((ld.json_ld[?"@type" == \'VideoObject\'].author.name | [0]), (ld.json_ld[?"@type" == \'VideoObject\'].author | [0])),
            scripts_ineligible_region: (ld.json_ld[?"@type" == \'VideoObject\'].ineligibleRegion | [0]),
            scripts_watch_action: (ld.json_ld[?"@type" == \'VideoObject\'].interactionStatistic[?ends_with(interactionType, \'WatchAction\')].userInteractionCount | [0][0]),
            scripts_like_action: (ld.json_ld[?"@type" == \'VideoObject\'].interactionStatistic[?ends_with(interactionType, \'LikeAction\')].userInteractionCount | [0][0]),
            config_i_meta_title: config_i_meta_title,
            config_i_meta_title_alt: config_i_meta_title_alt,
            config_i_meta_duration: config_i_meta_duration,
            config_i_meta_duration_alt: config_i_meta_duration_alt,
            scripts_page_link_canon: scripts_page_link_canon,
            i_rating_percent: i_rating_percent,
            i_votes_down: i_votes_down,
            i_favorites_counter: i_favorites_counter,
            straightLangUrl: straightLangUrl,
            saleVideo: VIDEO_SHOW.saleVideo,
            locked: VIDEO_SHOW.locked,
            playlistVideosUrl: VIDEO_SHOW.playlistVideosUrl,
            shareToStreamUrl: VIDEO_SHOW.shareToStreamUrl,
            isTrailerVideo: VIDEO_SHOW.isTrailerVideo,
            probillerUrl: VIDEO_SHOW.probillerUrl,
            isViewshare: VIDEO_SHOW.isViewshare,
            playlistRandomUrl: VIDEO_SHOW.playlistRandomUrl,
            controller: WIDGET_RATINGS_LIKE_FAV.controller,
            isFavourite: WIDGET_RATINGS_LIKE_FAV.isFavourite,
            pornstars: not_null(dom_pornstars, SUGGEST_PORNSTAR_BOX.pornstarNamesList[*].name),
            user_avatar: user_avatar,
            user_id: not_null(dom_user_id, dom_channel_id),
            user_type: user_type,
            upload_date: not_null((ld.json_ld[?"@type" == \'VideoObject\'].uploadDate | [0]), dom_video_info),
            can_stream: ADD_TO_MODAL.canStream
        }')
    };

// ============================================================================
// 4. NextVideo
// ============================================================================
next_video := Scraper()
    .select("html") {
        find_all("script", Value.Text, optional=true) | strip_comments | squish
            | js_blocks(["nextVideoPlaylistObject", "flashvars_*"])
    }
    .finalize {
        jmespath('not_null(flashvars.nextVideo, nextVideoPlaylistObject).{
            uploader_link: uploaderLink,
            next_url: nextUrl,
            duration: duration,
            channel_title: channelTitle,
            is_hd: isHD,
            title: title,
            thumb: thumb,
            rating: rating,
            vkey: vkey,
            views: views,
            viewsText: viewsText,
            badge: badge,
            isJoinPageEntry: isJoinPageEntry
        }')
    };

// ============================================================================
// 5. ScriptData
// ============================================================================
script_data := Scraper()
    .select("html") {
        find_all("script", Value.Text, optional=true) | strip_comments | squish | js_blocks(SCRAPER_VARS_NAMES)
    }
    .finalize {
        jmespath('merge(@, {
            model_profile_is_video_page: MODEL_PROFILE.isVideoPage,
            model_profile_url: MODEL_PROFILE.url,
            model_profile_token: MODEL_PROFILE.token,
            model_profile_username: MODEL_PROFILE.username,
            model_profile_link: MODEL_PROFILE.modelProfileLink,
            model_profile_share_title: MODEL_PROFILE.shareProfileTitle,
            model_profile_page_source: MODEL_PROFILE.pageSource,
            model_profile_is_premium: MODEL_PROFILE.isPremium,
            model_profile_viewkey: MODEL_PROFILE.viewkey,
            footer_traffic_junky_url: varObj_footer.trafficJunkyurl,
            footer_segment: varObj_footer.segment,
            footer_amateur_user: varObj_footer.amateurUser,
            subscribe_redirect_url: WIDGET_BUTTON_SUBSCRIBE.subscribeRedirectUrl,
            videoshow_watch_later_random_url: VIDEO_SHOW.watchLaterRandomUrl,
            videoshow_video_id: not_null(VIDEO_SHOW.videoId, VIDEO_SHOW.video_id),
            videoshow_feel_cache_url: VIDEO_SHOW.feelCacheUrl,
            videoshow_tag_suggestion_url: VIDEO_SHOW.tag_suggestion_url,
            videoshow_token: VIDEO_SHOW.token,
            videoshow_traffic_junky_url: VIDEO_SHOW.trafficJunkyurl,
            videoshow_track_video_id: VIDEO_SHOW.trackVideoId,
            videoshow_track_user_id: VIDEO_SHOW.trackUserId,
            videoshow_video_title_original: VIDEO_SHOW.videoTitleOriginal,
            videoshow_video_title_translated: VIDEO_SHOW.videoTitleTranslated,
            videoshow_vr: VIDEO_SHOW.vr,
            videoshow_category_suggestion_url: VIDEO_SHOW.categorySuggestionUrl,
            videoshow_watch_later_videos_url: VIDEO_SHOW.watchLaterVideosUrl,
            videoshow_is_premium: VIDEO_SHOW.isPremium,
            videoshow_video_title: VIDEO_SHOW.videoTitle,
            videoshow_vkey: VIDEO_SHOW.vkey,
            videoshow_fake_videos_url: VIDEO_SHOW.fakeVideosUrl,
            videoshow_fake_random_url: VIDEO_SHOW.fakeRandomUrl
        })')
    };

// ============================================================================
// 6. VideoBlock
// ============================================================================
video_block := Scraper()
    .select(".videoblock") {
        @"data-entrycode" | squish -> "data_entrycode"
        | @"data-id" | squish -> "data_id"
        | @"data-segment" | squish -> "data_segment"
        | @"data-video-id" | squish -> "data_video_id"
        | @"data-video-vkey" | squish -> "data_video_vkey"
        | @id | squish -> "video_dom_id"
        | @"data-video-type" | squish -> "data_video_type"
        | @"data-action" | squish -> "data_action"
        | fields({
            duration: "var.duration, .duration",
            pl_data_video_id: { sel: ".playlist-trigger[data-video-id], [data-video-id]", attr: "data-video-id" },
            user_name: ".usernameWrap a, .usernameWrap span",
            user_link: { sel: ".usernameWrap a", attr: "href" },
            video_link: { sel: ".title a, a.linkVideoThumb, a.img", attr: "href" },
            video_views: ".videoDetailsBlock .views var, .views var",
            video_rating: ".videoDetailsBlock .rating-container .value, .rating-container .value",
            video_added: ".videoDetailsBlock .added, .added",
            mediabook_url: { sel: "img[data-mediabook]", attr: "data-mediabook" },
            related_url: { sel: "a[data-related-url]", attr: "data-related-url" },
            is_hd: { sel: ".hd-thumbnail", type: exists }
        })
        | find_first("a.linkVideoThumb[title], .title a[title]", Value.Attribute, "title") -> "video_title"
        | find_first(".title a", Value.Text) -> "video_title"
        | find_first("img[src]", Value.Attribute, "src") -> "thumb_url"
        | find_first("img[data-thumb_url]", Value.Attribute, "data-thumb_url") -> "thumb_url"
        | find_first("img[data-src]", Value.Attribute, "data-src") -> "thumb_url"
        | find_first("img[data-image]", Value.Attribute, "data-image") -> "thumb_url"
    };

// ============================================================================
// 7. PreloadAd
// ============================================================================
let PRELOAD_URL_RE = "(//[^\"']+/_xa/ads_batch\\?[^\"']+|/_xa/ads_batch\\?[^\"']+)";

preload_ad := Scraper()
    .select("html") {
        find_all("script", Value.Text, optional=true) | filter(["tjPreloadAds", "adRollGlobalConfig"])
        | tee {
            filter(["tjPreloadAds"]) | js_block("JSON.parse")
            | regex_extract("^\\([\"'](.*)[\"']\\)$", 1) | unescape(false)
            | json_parse("ads", strict=false)
          }
        | strip_comments | squish | unescape(false) | regex_extract(PRELOAD_URL_RE, 1) | first(1)
        | query_params({
            clientType: "client_type",
            "channel[context_page_type]": "context_page_type",
            "channel[context_category]": "context_category",
            "channel[category]": "category",
            "channel[context_tag]": "context_tag",
            device_type: "device_type"
          })
        | query_param("channel[info]") | json_parse("info", strict=false)
    }
    .finalize {
        jmespath('{
            client_type: client_type,
            context_page_type: context_page_type,
            context_category: not_null(context_category, category),
            context_tag: context_tag,
            device_type: device_type,
            actor_id: info.actor_id,
            content_type: info.content_type,
            timestamp: info.timestamp,
            target_video_id: info.video_id,
            raw_ads: ads
        }')
    };

// ============================================================================
// 8. Category & Tag (VideoCategory / VideoTag)
// ============================================================================
video_categories_tags := Scraper()
    .select("html") {
        try_find("meta[data-context-category]", Value.Attribute, "data-context-category") | split(",") | squish | drop_empty | unique -> "meta_categories"
        | try_find("meta[data-context-tag]", Value.Attribute, "data-context-tag") | split(",") | squish | drop_empty | unique -> "meta_tags"
        | try_find(".categoriesWrapper .item a, .categoriesWrapper a.item", Value.Text) | squish | drop_empty | unique -> "dom_categories_list"
        | try_find(".tagsWrapper a.item, .tagsWrapper .item span, .tagsWrapper a", Value.Text) | squish | drop_empty | unique -> "dom_tags_list"
        | try_find(".pornstarsWrapper a.pstar-list-btn", Value.Text) | squish | drop_empty | unique -> "pornstars"
    }
    .select(".categoriesWrapper .item a, .categoriesWrapper a.item", "categories_dom") { use(link_map) }
    .select(".tagsWrapper a.item, .tagsWrapper a", "tags_dom") { use(link_map) }
    .select(".productionWrapper .item", "p_production") { use(link_map) }
    .select(".video-info-row .relatedSearchTermsSuggestion a", "r_relatedsts") { use(link_map) }
    .select(".langSpokenWrapper .item a", "l_langSpoken") { use(link_map) }
    .finalize {
        coalesce_keys({
            categories: ["meta_categories", "dom_categories_list"],
            tags: ["meta_tags", "dom_tags_list"]
        })
    };
