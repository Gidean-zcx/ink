/* $Date:: 2015-08-25 15:48:29 +0900#$ $Rev: 1098 $ */
(function() {
	"use strict";
	// /BUG Acrobat doesn't support descendant elements of button elements
	// Replace button elements with a elements (This should be executed as soon as possible)
	var testElement = document.createElement("div");
	testElement.id = "test-acrobat";
	document.body.appendChild(testElement);
	var isAcrobat = $(testElement).css("display") === "none";
	document.body.removeChild(testElement);

	if (isAcrobat) {
		var buttons = $(document.getElementById("menu")).find("button");
		for (var i = 0, len = buttons.length; i < len; i++) {
			var button = buttons[i];
			var text = button.textContent;
			var parent = button.parentNode;

			var a = document.createElement("a");
			var span = document.createElement("span");
			span.textContent = text;
			a.appendChild(span);
			parent.replaceChild(a, button);
		}
	}
})();

(function(OHP) {
	"use strict";
	OHP.Command = {
		REGION_L : "region was changed to L",
		REGION_S : "region was changed to S",
		RESIZE : "window was resized",
		CONTENT_CHANGED : "page content was changed",

		callbacks : [],
		register : function(object) {
			this.callbacks.push(object);
		},

		dispatch : function(message) {
			for (var i = 0, len = this.callbacks.length; i < len; i++) {
				this.callbacks[i].onmessage(message);
			}
		}
	};

	OHP.UA = {
		sHasMatchMedia : null,
		hasMatchMedia : function() {
			if (this.sHasMatchMedia === null) {
				this.sHasMatchMedia = "matchMedia" in window;
			}
			return this.sHasMatchMedia;
		},

		sHasGetComputedStyle : null,
		hasGetComputedStyle : function() {
			if (this.sHasGetComputedStyle === null) {
				this.sHasGetComputedStyle = "getComputedStyle" in window;
			}
			return this.sHasGetComputedStyle;
		},

		sSupportNS : null,
		supportNS : function() {
			if (this.sSupportNS === null) {
				this.sSupportNS = "createElementNS" in document;
			}
			return this.sSupportNS;
		},

		sHasStorage : null,
		hasStorage : function() {
			if (this.sHasStorage === null) {
				var hasStorage = false;
				if ("localStorage" in window) {
					// BUG Safari Private Browsing
					try {
						var storage = window.localStorage;
						var key = "ohp-test";
						storage.setItem(key, "");
						storage.removeItem(key);
						hasStorage = true;
					} catch (e) {
						hasStorage = false;
					}
				}
				this.sHasStorage = hasStorage;
			}
			return this.sHasStorage;
		},
	};

	OHP.Util = {
		KNOWN_DIRECTORIES : ["cover", "contents", "search"],
		sPathPrefix : null,
		resolvePath : function(path) {
			if (this.sPathPrefix === null) {
				this.sPathPrefix = "";
				var pathname = window.location.pathname;
				var directories = pathname.split("/");
				var lastDirectory = directories[directories.length - (pathname.match(/\.html$/) ? 2 : 1)];
				for (var i = 0, len = OHP.Util.KNOWN_DIRECTORIES.length; i < len; i++) {
					if (OHP.Util.KNOWN_DIRECTORIES[i] === lastDirectory) {
						this.sPathPrefix = "../";
						break;
					}
				}
			}
			return this.sPathPrefix + path;
		},

		sIsLRegion : null,
		MEDIA_QUERY : "screen and (max-width: 1279px)",
		useResizeToWatchRegionChange : false,
		isLRegion : function() {
			if (OHP.UA.hasMatchMedia()) {
				if (this.sIsLRegion === null) {
					var mq = window.matchMedia(this.MEDIA_QUERY);
					this.sIsLRegion = this.isLRegionMQ(mq);
					mq.addListener($.proxy(this.handleResize, this));
				}
				return this.sIsLRegion;
			} else if (OHP.UA.hasGetComputedStyle()) {
				if (this.sIsLRegion === null) {
					this.sIsLRegion = this.isLRegionCSS();
					this.useResizeToWatchRegionChange = true;
					this.watchResize();
				}
				return this.sIsLRegion;
			}
			return true;
		},

		isLRegionMQ : function(mq) {
			return !mq.matches;
		},

		isLRegionCSS : function() {
			var sub = document.getElementById("sub");
			return window.getComputedStyle(sub).position !== "static";
		},

		sWatchResize : false,
		watchResize : function() {
			if (!this.sWatchResize) {
				$(window).on("resize", $.proxy(this.handleResize, this));
				this.sWatchResize = true;
			}
		},

		handleResize : function(eventOrMQ) {
			var isLRegionNew;
			if ("type" in eventOrMQ && eventOrMQ.type === "resize") {
				OHP.Command.dispatch(OHP.Command.RESIZE);
				if (this.useResizeToWatchRegionChange) {
					isLRegionNew = this.isLRegionCSS();
				}
			} else {
				isLRegionNew = this.isLRegionMQ(eventOrMQ);
			}
			if (isLRegionNew !== undefined && isLRegionNew !== this.sIsLRegion) {
				this.sIsLRegion = isLRegionNew;
				OHP.Command.dispatch(isLRegionNew ? OHP.Command.REGION_L : OHP.Command.REGION_S);
			}
		},

		id : 0,
		generateID : function() {
			return "id_" + this.id++;
		},

		sManualNumber : null,
		getManualNumber : function() {
			if (this.sManualNumber === null) {
				this.sManualNumber = document.documentElement.getAttribute("data-manual-number");
			}
			return this.sManualNumber;
		},

		getStorageKey : function(key) {
			return this.getManualNumber() + "-" + key;
		},

		setItem : function(key, val) {
			if (!OHP.UA.hasStorage()) {
				return;
			}
			window.localStorage.setItem(this.getStorageKey(key), val);
		},

		getItem : function(key) {
			if (!OHP.UA.hasStorage()) {
				return null;
			}
			return window.localStorage.getItem(this.getStorageKey(key)) || null;
		},

		sTopicId : null,
		getTopicId : function() {
			if (this.sTopicId === null) {
				var parts = window.location.pathname.split("/");
				this.sTopicId = parts[parts.length - 1];
			}
			return this.sTopicId;
		},

		sPageType : null,
		getPageType : function() {
			if (this.sPageType === null) {
				var body = document.body;
				this.sPageType = body.getAttribute("data-page-type");
			}
			return this.sPageType;
		}
	};

	OHP.Debug = {
		isDebug : function() {
			return OHP.URL.getSearchParams().has("debug");
		}
	};

	OHP.SmoothScroller = {
		init : function() {
			this.running = false;
			this.hash = null;
			this.callback = null;
		},

		scrollToHash : function(hash, callback) {
			var target = null;
			var id = hash.substring(1);
			if (id === "top") {
				target = document.body;
			} else if (id !== "") {
				target = document.getElementById(id);
			}
			this.start(target, hash, callback);
		},

		scrollToElement : function(target, callback) {
			this.start(target, null, callback);
		},

		start : function(target, hash, callback) {
			if (this.running || target === null) {
				this.fail(callback);
				return;
			}
			this.running = true;
			this.hash = hash || null;
			this.callback = callback || null;
			this.jsStart(target);
		},

		fail : function(callback) {
			if (typeof callback === "function") {
				callback(false);
			}
		},

		done : function() {
			// Set hash before executing callback as callback may call focus()
			// http://src.chromium.org/viewvc/blink?view=revision&revision=183455
			if (this.hash !== null) {
				window.location.href = this.hash;
			}
			if (typeof this.callback === "function") {
				this.callback(true);
			}
			this.running = false;
		},

		scrollBox : $("html, body"),
		jsStart : function(target) {
			var $el = $(target);
			var offset = $el.offset();
			var self = this;
			this.scrollBox.animate({
				//scrollLeft : Math.round(offset.left),
				scrollTop : Math.round(offset.top)
			}).promise().done(function() {
				self.done();
			});
		}
	};
	OHP.SmoothScroller.init();

	function HeaderToggle(control, target) {
		this.control = control;
		this.target = target;
		this.isHidden = true;
		this.isBusy = false;
		this.initARIA();
		this.init();
	}

	HeaderToggle.prototype = {
		initARIA : function() {
			var id = this.target.getAttribute("id") || OHP.Util.generateID();
			this.target.setAttribute("id", id);
			this.control.setAttribute("role", "button");
			this.control.setAttribute("aria-controls", id);
		},

		init : function() {
			if (OHP.Util.isLRegion()) {
				this.control.setAttribute("aria-disabled", "true");
			} else {
				this.update();
			}
			$(this.control).on("click", $.proxy(this.handleEvent, this));
			OHP.Command.register(this);
		},

		handleEvent : function(e) {
			e.preventDefault();
			if (OHP.Util.isLRegion() || this.isBusy) {
				return;
			}
			this.isHidden = !this.isHidden;
			this.update();
		},

		update : function() {
			this.isBusy = true;
			var updated = $.proxy(this.updated, this);
			if (this.isHidden) {
				$(this.target).slideUp(updated);
			} else {
				$(this.target).slideDown(updated);
			}
		},

		updated : function() {
			var expanded = this.isHidden ? "false" : "true";
			this.control.setAttribute("aria-expanded", expanded);
			this.target.setAttribute("aria-expanded", expanded);
			this.isBusy = false;
		},

		onmessage : function(message) {
			if (message === OHP.Command.REGION_L) {
				this.control.setAttribute("aria-disabled", "true");
			} else if (message === OHP.Command.REGION_S) {
				this.control.setAttribute("aria-disabled", "false");
			}
		}
	};

	// Header
	(function() {
		var control = document.getElementById("header-search-toggle");
		var target = document.getElementById("header-search");
		if (control !== null && target !== null) {
			new HeaderToggle(control, target);
		}
	})();

	//
	// Menu
	//
	function MenuItem(menu, button, ul, level) {
		this.menu = menu;
		this.control = button;
		this.target = ul;
		this.level = level;
		var id = ul.id;
		button.setAttribute("aria-controls", id);
		this.li = button.parentNode;
		this.isHidden = true;
		// IE8 start
		this.fallbackImg = null;
		// IE8 end
		this.init();
		this.update();
	}

	MenuItem.prototype = {
		NS_SVG : "http://www.w3.org/2000/svg",
		init : function() {
			// IE8 start
			if (!OHP.UA.supportNS()) {
				var img = document.createElement("img");
				img.className = "js-icon";
				this.control.insertBefore(img, this.control.firstChild);
				this.fallbackImg = img;
				return;
			}
			// IE8 end
			var svg = document.createElementNS(this.NS_SVG, "svg");
			svg.setAttributeNS(null, "viewBox", "0 0 14 9");
			var title = document.createElementNS(this.NS_SVG, "title");
			this.title = title;
			svg.appendChild(title);
			var path = document.createElementNS(this.NS_SVG, "path");
			path.style.fill = "currentColor";
			this.path = path;
			svg.appendChild(path);
			this.control.insertBefore(svg, this.control.firstChild);
		},

		open : function() {
			this.isHidden = false;
			this.update();
		},

		close : function() {
			this.isHidden = true;
			this.update();
		},

		update : function() {
			var key = this.isHidden ? "close" : "open";
			var option = this.menu.option[key];
			var hidden = this.isHidden ? "true" : "false";
			if (!OHP.UA.supportNS()) {
				// IE8 start
				this.fallbackImg.src = this.level === "level1" ? option.fallbackImgLevel1 : option.fallbackImgLevel2;
				this.fallbackImg.alt = option.alt;
				// IE8 end
			} else {
				this.path.setAttributeNS(null, "d", option.path);
				this.title.textContent = option.alt;
			}
			// BUG: Android 4+
			this.target.setAttribute("data-hidden", hidden);
			this.li.setAttribute("data-hidden", hidden);
		}
	};

	function Menu(root, option) {
		this.root = root;
		this.option = option;
		this.init();
	}

	Menu.prototype = {
		init : function() {
			this.items = {};
			this.opened = {};
			this.initMenuItems();
			this.highlightedIds = [];
			this.initHighlight();
			this.initOpenness();
			$(this.root).on("click", $.proxy(this.handleEvent, this));
		},

		initMenuItems : function() {
			var $buttons = $("button", this.root);
			for (var i = 0, len = $buttons.length; i < len; i++) {
				var button = $buttons[i];
				var ul = $(button).next()[0];
				var level = $(button).closest("ul")[0].className;
				// ID start
				var id = "menu-" + i;
				ul.id = id;
				// ID end
				var item = new MenuItem(this, button, ul, level);
				this.items[id] = item;
			}
		},

		initHighlight : function() {
			var topicId = OHP.Util.getTopicId();
			var pageType = OHP.Util.getPageType();
			var key = "previousTopicId";
			var isTopicIdNative = true;
			if (pageType === "contents") {
				OHP.Util.setItem(key, topicId);
			} else {
				// index and search page: load previous topicId
				topicId = OHP.Util.getItem(key);
				isTopicIdNative = false;
			}
			this.topicId = topicId;

			// no previous topicId
			if (topicId === null) {
				return;
			}

			// Highlight ALL items
			var selector = "a[href*='" + topicId + "']";
			var $elements = $(selector, this.root);
			for (var i = 0, len = $elements.length; i < len; i++) {
				var element = $elements[i];
				element.className += " active";
				// jQuery's |$| returns non-live objects
				// |querySelectorAll| returns non-live NodeList
				// it's safe to remove href attribute here.
				if (isTopicIdNative) {
					element.removeAttribute("href");
				}
				var $span = $(element).find("span");
				$span.wrapInner(document.createElement("em"));
				var ids = this.getULs(element);
				this.highlightedIds.push(ids);
			}
		},

		getULs : function(element) {
			var ids = [];
			var parent = element.parentNode;
			while (parent !== null && parent !== this.root) {
				if (parent.nodeName.toLowerCase() === "ul" && parent.id !== "") {
					ids.push(parent.id);
				}
				parent = parent.parentNode;
			}
			return ids;
		},

		initOpenness : function() {
			// 1. Load previously opened menus
			var openedIds = this.loadState();

			// 2. Check whether the number of visible highlighted item is greater than or equal to 1
			var highlightedIds = this.highlightedIds;
			var hasVisibleHighlighted = false;
			for (var i = 0; i < highlightedIds.length; i++) {
				var ids = highlightedIds[i];
				var isHighlightedVisible = true;
				for (var j = 0; j < ids.length; j++) {
					if (!openedIds.hasOwnProperty(ids[j])) {
						isHighlightedVisible = false;
						break;
					}
				}
				if (isHighlightedVisible) {
					hasVisibleHighlighted = true;
					break;
				}

			}

			// 3. if so, do nothing. if not, open FIRST highlighted item
			if (!hasVisibleHighlighted && highlightedIds.length > 0) {
				ids = highlightedIds[0];
				for (i = 0; i < ids.length; i++) {
					openedIds[ids[i]] = true;
				}
			}

			// 4. open ALL of necessary items
			for (var id in openedIds) {
				if (openedIds.hasOwnProperty(id)) {
					this.open(id, true);
				}
			}
			this.saveState();
		},

		open : function(itemId, isBatchUpdate) {
			if (!this.items.hasOwnProperty(itemId)) {
				return;
			}
			this.items[itemId].open();
			this.opened[itemId] = true;
			if (!isBatchUpdate) {
				this.saveState();
			}
		},

		close : function(itemId, isBatchUpdate) {
			if (!this.items.hasOwnProperty(itemId)) {
				return;
			}
			this.items[itemId].close();
			delete this.opened[itemId];
			if (!isBatchUpdate) {
				this.saveState();
			}
		},

		handleEvent : function(e) {
			var target = e.target;
			while (target !== null && target !== this.root) {
				var nodeName = target.nodeName.toLowerCase();
				if (nodeName === "a") {
					return;
				}
				if (nodeName === "button") {
					break;
				}
				target = target.parentNode;
			}
			var itemId = target.getAttribute("aria-controls");
			if (this.opened.hasOwnProperty(itemId)) {
				this.close(itemId);
			} else {
				this.open(itemId);
			}
		},

		loadState : function() {
			var storeKey = "previousOpenedMenus";
			var value = OHP.Util.getItem(storeKey);
			// IE8 doesn't support JSON.parse
			// var ret = JSON.parse(value);
			var ret = {};
			if (value === null || value === "") {
				return ret;
			}
			var ids = value.split(",");
			for (var i = 0; i < ids.length; i++) {
				ret[ids[i]] = true;
			}
			return ret;
		},

		saveState : function() {
			var opened = this.opened;
			var storeKey = "previousOpenedMenus";
			// IE8 doesn't support JSON.stringfy
			// var value = JSON.stringfy(opened);
			var ids = [];
			for (var item in opened) {
				if (opened.hasOwnProperty(item)) {
					ids.push(item);
				}
			}
			var value = ids.join(",");
			OHP.Util.setItem(storeKey, value);
		}
	};

	(function() {
		var option = {
			"close" : {
				"alt" : "Open",
				"path" : "M0,0 L7,9 h1 L14,0 z",
				"fallbackImgLevel1" : OHP.Util.resolvePath("common/img/arrow_close_level1.png"),
				"fallbackImgLevel2" : OHP.Util.resolvePath("common/img/arrow_close.png")
			},
			"open" : {
				"alt" : "Close",
				"path" : "M0,9 L7,0 h1 L14,9 z",
				"fallbackImgLevel1" : OHP.Util.resolvePath("common/img/arrow_open_level1.png"),
				"fallbackImgLevel2" : OHP.Util.resolvePath("common/img/arrow_open.png")
			}
		};
		var root = document.getElementById("menu");
		var menu = new Menu(root, option);
		if (OHP.Debug.isDebug()) {
			OHP.Debug.Menu = menu;
		}
	})();

	// Fixed Pane
	function FixedPane(targetPane, referencePane) {
		targetPane.setAttribute("data-supported", "true");
		this.targetPane = targetPane;
		this.referencePane = referencePane;
		this.attr = "data-fixed";
		this.init();
		this.rAF = window.requestAnimationFrame || function(callback) {
				setTimeout(callback, 1000 / 60);
		};
	}

	FixedPane.prototype = {
		init : function() {
			OHP.Command.register(this);
			var func = $.proxy(this.handleEvent, this);
			$(window).on("scroll", func);
			$(window).on("load", func);
		},

		isBusy : false,
		handleEvent : function() {
			if (!OHP.Util.isLRegion() || this.isBusy) {
				return;
			}
			this.isBusy = true;
			var func = $.proxy(this.updated, this);
			this.rAF.call(window, func);
		},

		updated : function() {
			if (!this.isBusy) {
				return;
			}
			var targetPane = this.targetPane;
			var referencePane = this.referencePane;
			var referenceBox = referencePane.getBoundingClientRect();
			var fixed = referenceBox.top < 0;
			if (fixed !== this.fixed) {
				targetPane.setAttribute(this.attr, fixed ? "true" : "false");
				referencePane.setAttribute(this.attr, fixed ? "true" : "false");
				this.fixed = fixed;
			}
			var $targetPane = $(targetPane);
			var props = {
				height : "",
				transform : ""
			};
			if (!fixed) {
				var targetBox = targetPane.getBoundingClientRect();
				// Use |calc| here to avoid adding |resize| event handler
				props.height = "calc(100vh - " + targetBox.top + "px)";
			} else {
				// make fixed pane follow horizontal scroll
				var scrollX = window.scrollX || window.pageXOffset;
				props.transform = "translate(" + -scrollX + "px, 0)";
			}
			$targetPane.css(props);
			this.isBusy = false;
		},

		onmessage : function(message) {
			if (message === OHP.Command.CONTENT_CHANGED || message === OHP.Command.REGION_L) {
				this.handleEvent();
			}
		}
	};
	(function fixedPane() {
		if (!OHP.UA.hasGetComputedStyle()) {
			return;
		}
		var sub = document.getElementById("sub");
		var main = document.getElementById("main");
		if (sub !== null && main !== null) {
			new FixedPane(sub, main);
		}
	})();

	// Return Page Top
	function ReturnPageTop(target) {
		this.target = target;
		this.init();
	}

	ReturnPageTop.prototype = {
		CLASS_NAME : "visible",

		init : function() {
			this.fixed = false;
			this.update();
			$(window).on("scroll", $.proxy(this.update, this));
			OHP.Command.register(this);
		},

		update : function() {
			if (OHP.Util.isLRegion()) {
				return;
			}
			var fixed = (window.scrollY || window.pageYOffset) > 0;
			if (fixed !== this.fixed) {
				// BUG IE9 doesn't support classList
				$(this.target).toggleClass(this.CLASS_NAME, fixed);
				this.fixed = fixed;
			}
		},

		onmessage : function(message) {
			if (message === OHP.Command.REGION_S) {
				this.update();
			}
		}
	};
	(function() {
		var returnpagetops = $(".return-pagetop-s");
		for (var i = 0, len = returnpagetops.length; i < len; i++) {
			new ReturnPageTop(returnpagetops[i]);
		}
	})();


	// Table Scroller
	function TableScroller(container) {
		this.container = container;
		var $scroller = $(container).children(".table-scroller");
		this.scroller = $scroller[0];
		var $table = $scroller.children("table");
		this.table = $table[0];
		this.checkOverflow();
		this.checkShadow();
		$scroller.on("scroll", $.proxy(this.checkShadow, this));
		OHP.Command.register(this);
	}

	TableScroller.prototype = {
		checkOverflow : function() {
			if ($(this.container).width() < $(this.table).width()) {
				this.container.setAttribute("data-overflow", "true");
			} else {
				this.container.removeAttribute("data-overflow");
			}
		},

		checkShadow : function() {
			var scroller = this.scroller;
			var scrollLeftMax = scroller.scrollWidth - scroller.clientWidth;
			var isRTL = document.documentElement.getAttribute("dir") === "rtl";
			// Chrome behaviour
			if (scrollLeftMax === 0 || (isRTL && (scroller.scrollLeft === 0)) || (!isRTL && (scroller.scrollLeft === scrollLeftMax))) {
				this.container.removeAttribute("data-shadow");
			} else {
				this.container.setAttribute("data-shadow", "true");
			}
		},

		onmessage : function(message) {
			if (message === OHP.Command.RESIZE) {
				this.checkOverflow();
				this.checkShadow();
			}
		}
	};

	// ToggleBox
	function ToggleBox(control, target, option) {
		this.control = control;
		this.target = target;
		this.option = option || {};
		this.isHidden = true;
		this.initARIA();
		this.init();
	}

	ToggleBox.prototype = {
		initARIA : function() {
			var id = this.target.getAttribute("id") || OHP.Util.generateID();
			this.target.setAttribute("id", id);
			this.control.setAttribute("role", "button");
			this.control.setAttribute("aria-controls", id);
		},

		init : function() {
			var img = document.createElement("img");
			img.className = "js-icon";
			this.control.appendChild(img);
			this.img = img;

			if (OHP.Util.isLRegion()) {
				this.control.setAttribute("aria-disabled", "true");
			} else {
				this.update();
			}

			var handler = $.proxy(this.handleEvent, this);
			$(this.control).on("click", handler);
			if (this.option.needKeyboardSupport) {
				$(this.control).on("keypress", handler);
			}
			OHP.Command.register(this);
		},

		handleEvent : function(e) {
			if (e.type === "click") {
				e.preventDefault();
				this.toggle();
			} else if (e.type === "keypress") {
				if (("key" in e && e.key === "Enter") || e.which === 13) {
					this.toggle();
				}
			}
		},

		toggle : function() {
			if (OHP.Util.isLRegion()) {
				return;
			}
			this.isHidden = !this.isHidden;
			this.update();
		},

		update : function() {
			var expanded = this.isHidden ? "false" : "true";
			this.control.setAttribute("aria-expanded", expanded);
			this.target.setAttribute("aria-expanded", expanded);
			var key = this.isHidden ? "close" : "open";
			var option = this.option[key];
			this.img.src = option.src;
			this.img.alt = option.alt;
		},

		onmessage : function(message) {
			if (message === OHP.Command.REGION_L) {
				this.control.setAttribute("aria-disabled", "true");
				// L Region: panel is always opened
				this.control.removeAttribute("aria-expanded");
				this.target.removeAttribute("aria-expanded");
			} else if (message === OHP.Command.REGION_S) {
				this.control.setAttribute("aria-disabled", "false");
				this.update();
			}
		}
	};

	(function() {
		var option = {
			close : {
				"alt" : "Open",
				"src" : OHP.Util.resolvePath("common/img/btn_namelist_close.png")
			},
			open : {
				"alt" : "Close",
				"src" : OHP.Util.resolvePath("common/img/btn_namelist_open.png")
			},
			needKeyboardSupport : true
		};
		var content = document.getElementById("id_content");
		var $nameitems = $(".name-list > li", content);
		for (var i = 0, len = $nameitems.length; i < len; i++) {
			var $children = $($nameitems[i]).children();
			if ($children.length === 1) {
				continue;
			}
			var control = $children.first()[0];
			var target = $children.last()[0];
			control.tabIndex = 0;
			new ToggleBox(control, target, option);
		}

		var $tables = $(".table-container", content);
		for (i = 0, len = $tables.length; i < len; i++) {
			var table = $tables[i];
			new TableScroller(table);
		}
		if ($tables.length) {
			OHP.Util.watchResize();
		}
	})();

	// Smooth Scroll
	(function() {
		var sLocation = window.location.href.split("#")[0];
		var sLocationRegExp = new RegExp("(.+)#.+$");
		function isInternalLink(element) {
			// BUG IE8-11
			element.href += "";
			var hrefResult = sLocationRegExp.exec(element.href);
			return hrefResult !== null && hrefResult[1] === sLocation;
		}

		function clickHandler(e) {
			var target = e.currentTarget;
			if (e.isDefaultPrevented()) {
				return;
			}
			e.preventDefault();
			OHP.SmoothScroller.scrollToHash(target.hash, null);
		}

		var links = document.links;
		for (var i = 0, len = links.length; i < len; i++) {
			var link = links[i];
			if (isInternalLink(link)) {
				$(link).on("click", clickHandler);
			}
		}
	})();

	(function lastChildFix() {
		var hasLastChild = false;
		try {
			hasLastChild = document.querySelector(":last-child") !== null;
		} catch (e) {
			// Do nothing
		}
		if (hasLastChild) {
			return;
		}

		var selectorsDict = {
			"id_content" : [".step-list > li:last-child", ".step-list .step-desc:last-child", ".name-list > li:last-child"],
			"menu" : [".level1 > li:last-child", ".level2 > li:last-child", ".level3 > li:last-child"]
		};

		for (var rootId in selectorsDict) {
			if (!selectorsDict.hasOwnProperty(rootId)) {
				continue;
			}
			var $root = $(document.getElementById(rootId));
			var selectors = selectorsDict[rootId];
			for (var i = 0, len = selectors.length; i < len; i++) {
				$root.find(selectors[i]).addClass("last-child");
			}
		}
	})();
})(window.OHP = window.OHP || {});
