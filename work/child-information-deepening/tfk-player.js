var readAlongScriptSrc;
var readAlongScriptId;
var readAlong;
var apiDomain = "api.readalong.ai";
var players = [];

function ReadAlong(scriptId) {
    var apiGetContentUrl = `https://${apiDomain}/api/getcontent`;
    var apiPostContentUrl = `https://${apiDomain}/api/postcontent`;
    var apiContentErrorUrl = `https://${apiDomain}/api/contenterror`;
    var apiReadAlongEventUrl = `https://${apiDomain}/api/readalongevent`;


    var parent = this;
    this.readAlongRequest = {};
    this.readAlongRequest.scriptId = scriptId;
    this.readAlongRequest.pageUrl = window.location.href;

    let readAlongScript = document.getElementById("readAlongScript");
    if (readAlongScript !== null) {
        let articleId = readAlongScript.getAttribute("articleid");
        if (isValidGUID(articleId)) {
            this.readAlongRequest.articleId = articleId;
        }
    }

    var audioScrollElement = null;
    var howler = null;
    var soundId = null;
    var progressSliderDown = false;
    var volumeSliderDown = false;
    var speedNavFontFamilyOn = "Nunito Sans";
    var speedNavFontFamilyOff = "Nunito Sans";
    var speedNavFontWeightOn = 500;
    var speedNavFontWeightOff = 400;
    var speedNavColorOn = "#313131";
    var speedNavColorOff = "#7C8084";
    var lastHighlight = Date.now();
    var highlightedElement;
    var highlightColor;
    var autoScroll;
    var scrollOffsetMin;
    var isTouch = (("ontouchstart" in window) || (navigator.msMaxTouchPoints > 0));
    var tallySafe = false;
    var lastListen = 0;
    var listenTime = 0;
    var playRate = 1.0;
    var hasPlayed = false;
    var focusPauseOnPlay = false;
    var listening = false;
    var audioOnly = false;
    var hasMiniPlayer = false;
    var timerCountdown = false;
    var seekLockUntil = 0;
    var seekTarget = null;
    var speedSelectionElements;
    var inlineElements = {};
    var inlineElementsList = [
        "#text", "a", "abbr", "acronym", "b", "bdo", "big", "br", "button", "cite", "code", "dfn", "em", "i", "iframe",
        "img", "input", "kbd", "label", "map", "object", "output", "q", "samp", "script", "select", "small", "span",
        "strong", "sub", "sup", "textarea", "time", "tt", "var"
    ];

    for (var i = 0; i < inlineElementsList.length; ++i)
        inlineElements[inlineElementsList[i].toUpperCase()] = 1;
    var excludedElements = {};
    var excludedElementsList = ["script", "noscript", "style", "button", "cite", "figcaption"];
    for (var i = 0; i < excludedElementsList.length; ++i)
        excludedElements[excludedElementsList[i].toUpperCase()] = 1;

    var contentConfig;
    var contentId;
    var readAlongEvent = {};

    var initRequirements = {};
    initRequirements.pageWithinDateRange = true;
    initRequirements.pageContainsShortCircuitElements = false;


    this.start = function() {
        // Correct promise chaining, don't nest the then() calls
        fetch(apiGetContentUrl,
            {
                method: "POST",
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(parent.readAlongRequest)
            })
            .then(res => res.json())
            .then(readAlongResponse => {
                parent.readAlongRequest.response = readAlongResponse;
                if (readAlongResponse.status || !readAlongResponse.contentConfig)
                    return;
                setInitRequirements(readAlongResponse);
                if (readAlongResponse.postHtml && initRequirements.pageWithinDateRange && !initRequirements.pageContainsShortCircuitElements && !readAlongResponse.contentLimitReached) {
                    postHtml(readAlongResponse);
                } else if (readAlongResponse.enabled && !initRequirements.pageContainsShortCircuitElements) {
                    initializeReadAlong(readAlongResponse);
                }
            }).catch(error => {
                //suppressed 
            });
    };

    function isValidGUID(guid) {
        var regex = /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/i;
        return regex.test(guid);
    }

    function setInitRequirements(readAlongResponse) {
        try {
            if (readAlongResponse.contentConfig.shortCircuitSelectors)
                locateShortCircuitElements(readAlongResponse.contentConfig.shortCircuitSelectors);
        } catch (e) {
        }
        try {
            if (readAlongResponse.contentConfig.minPublishDate && readAlongResponse.contentConfig.dateSelector) {
                initRequirements.pageWithinDateRange = false;
                if (readAlongResponse.contentConfig.dateRegEx)
                    locatePageCreationDate(readAlongResponse.contentConfig.minPublishDate,
                        readAlongResponse.contentConfig.dateSelector,
                        readAlongResponse.contentConfig.dateRegEx);
                else if (readAlongResponse.contentConfig.dateAttribute)
                    locatePageCreationDate(readAlongResponse.contentConfig.minPublishDate,
                        readAlongResponse.contentConfig.dateSelector,
                        undefined,
                        readAlongResponse.contentConfig.dateAttribute);
                else
                    locatePageCreationDate(readAlongResponse.contentConfig.minPublishDate,
                        readAlongResponse.contentConfig.dateSelector);
            }
        } catch (e) {
        }
    };

    function locateShortCircuitElements(selectorList) {
        const selectors = selectorList.split("|");
        initRequirements.pageContainsShortCircuitElements = selectors.some(function(selector) {
            try {
                let el;
                el = document.evaluate(selector, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null)
                    .singleNodeValue;
                if (el) {
                    return true;
                }
            } catch (e) {
            }
            return false;
        });
    };

    function locatePageCreationDate(minPublishDate, dateSelector, dateRegex, dateAttribute) {
        let articleDate;
        const dateNode = document.evaluate(dateSelector, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null)
            .singleNodeValue;

        //1st attempt: config-defined regex
        if (dateRegex !== undefined) {
            try {
                const regex = new RegExp(dateRegex, "g");
                const found = dateNode.textContent.match(regex);
                if (found) {
                    articleDate = Date.parse(found[0]);
                }
            } catch (ex) {
            }
        }

        //2nd attempt: attribute value
        if (dateAttribute !== undefined && (articleDate == null || Number.isNaN(articleDate))) {
            try {
                const found = dateNode.getAttribute(dateAttribute);
                if (found) {
                    articleDate = Date.parse(found);
                }
            } catch (ex) {
            }
        }

        //3rd attempt: just let Date.parse try on the raw textContent
        if (articleDate == null || Number.isNaN(articleDate)) {
            try {
                articleDate = Date.parse(dateNode.textContent.trim()); //either a Date or NaN
            } catch (ex) {
            }
        }

        //4th attempt: generic mm/dd/yyyy parsing
        if (articleDate == null || Number.isNaN(articleDate)) {
            try {
                const regex = /(\d{1, 2}[\/|-]\d{1, 2}[\/|-]\d{2, 4})/g;
                const found = dateNode.textContent.match(regex);
                if (found) {
                    articleDate = Date.parse(found[0]);
                }
            } catch (e) {
            }
        }

        //finally
        if (articleDate == null || Number.isNaN(articleDate))
            return; //couldn't parse with any method

        const minPublishDateObj = Date.parse(minPublishDate);
        if (articleDate >= minPublishDateObj)
            initRequirements.pageWithinDateRange = true;
    };

    function initializeReadAlong(readAlongResponse) {
        try {
            contentConfig = readAlongResponse.contentConfig;
            contentId = readAlongResponse.contentId;

            autoScroll = contentConfig.autoScroll;
            scrollOffsetMin = contentConfig.scrollOffsetMin;
            highlightColor = contentConfig.highlightColor;

            readAlongEvent.contentConfigId = contentConfig.contentConfigId;
            readAlongEvent.contentId = contentId;
            readAlongEvent.sessionId = readAlongResponse.sessionId;
            readAlongEvent.timeSpentQuartile = -1;

            try {
                tagBodyForReadAlong(readAlongResponse.contentSelectorList,
                    readAlongResponse.exclusionSelectorList,
                    readAlongResponse.paragraphTimes, contentConfig.useAllContentSelectorResults);
            } catch (ex) {
                audioOnly = true;
                sendContentError("");
            }


            const audioLauncher = document.createElement("button");
            audioLauncher.id = "audioLauncher";
            audioLauncher.type = "button";
            audioLauncher.setAttribute("aria-label", "Listen to audio");
            audioLauncher.style.display = "flex";
            audioLauncher.style.alignItems = "center";
            audioLauncher.style.cursor = "pointer";
            audioLauncher.style.background = "none";
            audioLauncher.style.border = "none";
            audioLauncher.style.padding = "0";
            placeAudioElement(audioLauncher, contentConfig.launchTypeId, contentConfig.launchSelector);
            audioLauncher.outerHTML = readAlongResponse.audioLauncherHtml;

            const audioPlayerContainer = document.createElement("div");
            placeAudioElement(audioPlayerContainer, contentConfig.playerLaunchTypeId, contentConfig.playerSelector);
            audioPlayerContainer.outerHTML = readAlongResponse.audioPlayerHtml;

            if (readAlongResponse.miniPlayerHtml && readAlongResponse.miniPlayerHtml.length > 100) {
                hasMiniPlayer = true;
                const miniPlayer = document.createElement("div");
                placeAudioElement(miniPlayer, 5, "body");
                miniPlayer.outerHTML = readAlongResponse.miniPlayerHtml;
            }

            const howlerScript = document.createElement("script");
            document.body.appendChild(howlerScript);
            howlerScript.src = "https://assets.emagazines.com/uploads/howler_07566d58-11d2-48f8-bad6-b623be54fd75.js";

            submitEvent();

        } catch (ex) {
            //send up error, we have to bail on the whole audio experience
            //short-circuits the content tagging and player initialization
            sendContentError(`Error setting up read-along player: ${ex.message}`);
            return;
        }

        initAudioPlayer();
    }

    function sendContentError(errorMessage) {
        const contentError = {
            "scriptId": contentConfig.scriptId,
            "contentId": contentId,
            "audioOnly": audioOnly,
            "errorMessage": errorMessage
        };
        fetch(apiContentErrorUrl,
            {
                method: "POST",
                headers: { 'Content-Type': "application/json" },
                body: JSON.stringify(contentError)
            }).then(res => {}).catch(error => {});
    }

    var paragraphIndex = 0;
    var paragraphNode = null;

    function tagBodyForReadAlong(contentSelectorList, exclusionSelectorList, paragraphTimes, useAllContentSelectorResults) {
        const excludedNodes = [];

        for (let es = 0; es < exclusionSelectorList.length; ++es) {
            const exclusionSelector = exclusionSelectorList[es];
            const exclusionResult = document.evaluate(exclusionSelector, document, null, XPathResult.ANY_TYPE, null);
            let excludedNode = null;
            while (true) {
                try {
                    excludedNode = exclusionResult.iterateNext();
                } catch (e) {
                    excludedNode = null;
                }
                if (excludedNode) {
                    excludedNodes.push(excludedNode);
                } else
                    break;
            }
        }

        for (let i = 0; i < excludedNodes.length; ++i) {
            const excludedNode = excludedNodes[i];
            excludedNode.setAttribute("skip", "1");
        }

        paragraphIndex = 0;
        for (let cs = 0; cs < contentSelectorList.length; ++cs) {
            const contentSelector = contentSelectorList[cs];
            const contentIterator = document
                .evaluate(contentSelector, document, null, XPathResult.ORDERED_NODE_ITERATOR_TYPE, null);

            if (!contentIterator)
                return;

            var contentNodes = [];
            var cn = contentIterator.iterateNext();
            while (cn) {
                contentNodes[contentNodes.length] = cn;
                cn = contentIterator.iterateNext();
            }

            if (contentNodes && contentNodes.length > 0) {
                if (!useAllContentSelectorResults) {
                    var contentNode = contentNodes[0];
                    contentNodes = [contentNode];
                }
                for (var i = 0; i < contentNodes.length; ++i) {
                    var contentNode = contentNodes[i];
                    addAudioParagraphs(contentNode, paragraphTimes);
                }
            }
        }
    }

    var taggedElements = [];

    function descendantOf(testElement, previouslyTaggedElement, index, array) {
        if (!previouslyTaggedElement)
            return false;
        return testElement.contains(previouslyTaggedElement);
    }

    function addAudioParagraphs(node, paragraphTimes) {
        if (paragraphIndex >= paragraphTimes.length)
            return;

        if (node.attributes) {
            const skip = node.getAttribute("skip");
            if (skip) {
                console.log("got one!");
                return;
            }
        }
        if (node.nodeName.toUpperCase() == "#TEXT") {
            const text = node.nodeValue.replace("&nbsp;", " ");
            if (!isNullOrWhitespace(text)) {
                const ancestor = getParagraphAncestor(node);
                if (ancestor !== paragraphNode) {
                    if (
                        paragraphTimes[paragraphIndex] !== -1) {
                        /* We want to see if the time is one that doesn't really exist in the audio. AKA The paragraph ahs been entirely ignored. */
                        //starting a new paragraph
                        paragraphNode = ancestor;

                        //If none of the previously tagged elements are a descendant of the one we're about to tag, it's okay to proceed
                        if (!taggedElements.some(descendantOf.bind(null, paragraphNode))) {
                            paragraphNode.classList.add("readAlong");
                            paragraphNode.setAttribute("audStart", `p-w_${paragraphTimes[paragraphIndex]}`);
                            taggedElements.push(paragraphNode);
                        }
                    }
                    paragraphIndex++;
                }
            }
        }
        else if (contentConfig.readImageAltText && node.nodeName.toUpperCase() == "IMG") {
            var altText = node.getAttribute("alt");
            if (altText && altText.length > 0) {
                paragraphNode = node;
                paragraphNode.classList.add("readAlong");
                paragraphNode.setAttribute("audStart", `p-w_${paragraphTimes[paragraphIndex]}`);
                taggedElements.push(paragraphNode);
                paragraphIndex++;
            }
        }
        else {
            for (let i = 0; i < node.childNodes.length; ++i) {
                const cn = node.childNodes[i];
                //skip scripts and styles
                if (excludedElements[cn.nodeName.toUpperCase()])
                    continue;
                else
                    addAudioParagraphs(cn, paragraphTimes);
            }
        }
    }

    function getParagraphAncestor(node) {
        var ancestor = node.parentNode;
        while (true) {
            if (!inlineElements[ancestor.nodeName.toUpperCase()])
                break;
            ancestor = ancestor.parentNode;
        }
        return ancestor;
    }

    function isNullOrWhitespace(text) {
        return !text || !text.trim();
    }

    //function tagBodyForReadAlong(contentHtmlList) {
    //    for (var i = 0; i < contentHtmlList.length; ++i) {
    //        var contentHtml = contentHtmlList[i];
    //        var contentSelector = contentHtml.contentSelector;
    //        var taggedHtml = contentHtml.taggedHtml;
    //        var contentNode = document.evaluate(contentSelector, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
    //        const parser = new DOMParser();
    //        const virtualDom = parser.parseFromString(taggedHtml, "text/html");
    //        const virtualContentNode = virtualDom.body.firstChild;
    //        recursiveTag(virtualContentNode, contentNode);
    //    }
    //}

    //function recursiveTag(vNode, node) {
    //    if (vNode.classList && vNode.classList.contains("read_along_wrap")) {
    //        var readAlongWrapper = document.createElement("div");
    //        node.parentElement.insertBefore(readAlongWrapper, node);
    //        var wrappedNode = node;
    //        for (var i = 0; i < vNode.childNodes.length; ++i) {
    //            var vChild = vNode.childNodes[i];
    //            if (vChild.nodeName != wrappedNode.nodeName)
    //                console.log("what?");

    //            var nextNode = wrappedNode.nextSibling;
    //            wrappedNode.remove();
    //            var copyNode = vChild.cloneNode(true);
    //            readAlongWrapper.appendChild(copyNode);
    //            wrappedNode = nextNode;
    //        }
    //        node = readAlongWrapper;
    //    }
    //    if (vNode.classList && vNode.classList.contains("readAlong")) {
    //        node.classList.add("readAlong");
    //        const audStart = vNode.getAttribute("audStart");
    //        node.setAttribute("audStart", audStart);
    //    }
    //    //check if the structure changed
    //    if (vNode.childNodes.length != node.childNodes.length)
    //        console.log("ReadAlong Structure mismatch!!!");
    //    for (var i = 0; i < vNode.childNodes.length; ++i) {
    //        var vChild = vNode.childNodes[i];
    //        var child = node.childNodes[i];
    //        if (child && vChild)
    //            recursiveTag(vChild, child);
    //    }
    //}

    function placeAudioElement(audioLauncher, launchType, launchSelector) {
        var launchElement = document.evaluate(launchSelector, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null)
            .singleNodeValue;
        if (launchSelector == "body")
            launchElement = document.body;
        if (launchType == 2) { //before
            launchElement.parentElement.insertBefore(audioLauncher, launchElement);
        } else if (launchType == 3) { //after
            launchElement.parentNode.insertBefore(audioLauncher, launchElement.nextSibling);
        } else if (launchType == 4 || launchType == 5) { //inside and float
            launchElement.insertBefore(audioLauncher, launchElement.firstChild);
        }
    }

    function postHtml(readAlongResponse) {
        const contentElement = document.body;

        // Get title node using titleSelector
        const docTitle = document.evaluate(readAlongResponse.contentConfig.titleSelector,
            document,
            null,
            XPathResult.FIRST_ORDERED_NODE_TYPE,
            null);
        let titleNode = docTitle && docTitle.singleNodeValue;

        // If title node is not found, get it from the <title> tag
        if (!titleNode) {
            titleNode = document.querySelector("title");
        }

        // Update parent request title
        parent.readAlongRequest.title = titleNode ? titleNode.innerText : window.location.pathname;

        // Get publish date
        const dateNode = document.evaluate(readAlongResponse.contentConfig.dateSelector,
            document,
            null,
            XPathResult.FIRST_ORDERED_NODE_TYPE,
            null).singleNodeValue;

        // Update parent request publish date
        parent.readAlongRequest.publishDateString = dateNode ? dateNode.innerText : "";

        // Update parent request HTML
        parent.readAlongRequest.Html = contentElement.outerHTML;

        // Fetch API
        fetch(apiPostContentUrl, {
                method: "POST",
                headers: { 'Content-Type': "application/json" },
                body: JSON.stringify(parent.readAlongRequest)
            })
            .then(res => {

            }) // do something with the response if needed
            .catch(error => {

            }); // handle error if needed
    }

    function checkSendReadAlongEvent() {
        try { //don't let analytics kill read-along
            const dateNow = Date.now();
            const now = dateNow / 1000;

            if (lastListen === 0) {
                lastListen = now;
            }

            const timeDelta = now - lastListen;
            listenTime += timeDelta * playRate;
            lastListen = now;
            const timePercent = 100 * (listenTime / howler.duration());
            let timeSpentQuartile = 0;
            if (timePercent >= 25) timeSpentQuartile = 25;
            if (timePercent >= 50) timeSpentQuartile = 50;
            if (timePercent >= 75) timeSpentQuartile = 75;
            if (timePercent >= 95) timeSpentQuartile = 100;

            if (timeSpentQuartile > readAlongEvent.timeSpentQuartile) {
                readAlongEvent.timeSpentQuartile = timeSpentQuartile;
                submitEvent();
            }
        } catch (ex) {
        }
    }

    function submitEvent() {
        fetch(apiReadAlongEventUrl,
            {
                method: "POST",
                headers: { 'Content-Type': "application/json" },
                body: JSON.stringify(readAlongEvent)
            }).then(res => {
        }).catch(error => {});
    };

    function updateMiniPlayerProgress(pct) {
        const left = document.getElementsByClassName("ra-leftProgress")[0];
        const right = document.getElementsByClassName("ra-rightProgress")[0];
        const leftRotate = Math.min(Math.round(pct * 360, 0), 180);
        left.style.transform = `rotate(${leftRotate}deg)`;
        if (pct > 0.5) {
            right.style.transform = `rotate(${(pct - 0.5) * 360}deg)`;
        } else {
            right.style.transform = "rotate(0deg)";
        }
    }

    function initAudioPlayer() {
        const elms = [
            "miniPlayBtn", "miniPauseBtn", "autoScrollCheckbox", "autoScrollLabel", "autoScrollCheckMark", "miniPlayer",
            "audioLauncher", "audioLauncherContainer", "audioPlayerContainer", "navMain", "audioPlayer",
            "audioClose", "audioCloseBtn",
            "navAudio", "tocLastPageButton", "navAudioActive", "currentTime", "timeDivider", "duration", "playBtn",
            "pauseBtn", "playPauseBtn", "progressControl", "loadBtn",
            "progressDot", "emptyProgressBar", "backgroundProgressBar", "progressBar", "speedControl", "speedNav",
            "volumeEditor", "volumeNav", "volumeNavBtn", "volumeControl", "volumeBarEmpty", "volumeOverlay", "volumeDot",
            "progressRange", "volumeRange"
        ];

        elms.forEach(function(elm) {
            window[elm] = document.getElementById(elm);
        });

        audioCloseBtn.addEventListener('click',
            function () {
                clearAudio();
            });

        volumeNavBtn.addEventListener('click',
            function () {
                if (window.volumeEditor.style.display == "flex") {
                    window.volumeEditor.style.display = "none";
                } else {
                    window.volumeEditor.style.display = "flex";
                    window.speedControl.style.display = "none";
                    requestAnimationFrame(function () {
                        if (window.volumeRange) {
                            window.volumeRange.focus();
                        }
                    });
                }
            });


        window.audioLauncher.addEventListener("click",
            function() {
                audioLauncherContainer.style.display = "none";
                audioPlayerContainer.style.display = "flex";
                if (miniPlayer)
                    miniPlayer.style.display = "block";
                listening = true;
                focusPauseOnPlay = true;
                showAudioNav();
            });

        playPauseBtn.addEventListener('click',
            function () {
                closeToasterEditors();
                if (playBtn.style.display !== 'none') {
                    play();
                } else {
                    parent.pause();
                }
            });

        if (miniPlayBtn) {
            miniPlayBtn.addEventListener("click",
                function() {
                    closeToasterEditors();
                    play();
                });
        }

        if (miniPauseBtn) {
            miniPauseBtn.addEventListener("click",
                function () {
                    hasPlayed = false;
                    closeToasterEditors();
                    parent.pause();
                });
        }

        if (autoScrollCheckbox) {
            if (autoScroll) {
                autoScrollCheckbox.checked = true;
            } else {
                autoScrollCheckbox.checked = false;
            }
            autoScrollCheckbox.addEventListener("click",
                function() {
                    //toggle
                    if (autoScroll) {
                        autoScroll = false;
                        autoScrollCheckbox.style.borderColor = "#989898";
                        autoScrollLabel.style.color = "#989898";
                        autoScrollCheckMark.style.display = "none";
                    } else {
                        autoScroll = true;
                        autoScrollCheckbox.style.borderColor = "#1EA1F1";
                        autoScrollLabel.style.color = "#000";
                        autoScrollCheckMark.style.display = "inline";
                    }
                });
        }

        speedNav.addEventListener("click",
            function(event) {
                displaySpeedNav();
            });

        speedSelectionElements = document.getElementsByClassName("speedSelection");
        for (let i = 0; i < speedSelectionElements.length; i++) {
            const el = speedSelectionElements[i];

            el.addEventListener("click",
                function(event) {
                    adjustRate(el, el.dataset.rate);
                });
        }

        backgroundProgressBar.addEventListener("mousedown",
            function() {
                if (isTouch) {
                    return;
                }
                progressSliderDown = true;
                const pct = event.layerX / parseFloat(backgroundProgressBar.scrollWidth);
                adjustProgress(pct);
            });

        backgroundProgressBar.addEventListener("touchstart",
            function() {
                progressSliderDown = true;
                const rect = event.target.getBoundingClientRect();
                const x = event.targetTouches[0].pageX - rect.left;
                const pct = x / parseFloat(backgroundProgressBar.scrollWidth);
                adjustProgress(pct);
            });

        backgroundProgressBar.addEventListener("mouseup",
            function() {
                if (isTouch) {
                    return;
                }
                progressSliderDown = false;
            });

        backgroundProgressBar.addEventListener("touchend",
            function() {
                progressSliderDown = false;
            });

        backgroundProgressBar.addEventListener("blur",
            function() {
                progressSliderDown = false;
            });

        backgroundProgressBar.addEventListener("mousemove",
            function() {
                if (isTouch) {
                    return;
                }
                if (progressSliderDown) {
                    const pct = event.layerX / parseFloat(backgroundProgressBar.scrollWidth);
                    adjustProgress(pct);
                }
            });

        backgroundProgressBar.addEventListener("touchmove",
            function() {
                if (progressSliderDown) {
                    const rect = event.target.getBoundingClientRect();
                    const x = event.targetTouches[0].pageX - rect.left;
                    const pct = x / parseFloat(backgroundProgressBar.scrollWidth);
                    adjustProgress(pct);
                }
            });

        // Native range input for progress slider - works with all screen readers
        if (progressRange) {
            progressRange.addEventListener('input', function () {
                if (!howler) return;
                var howlerDuration = howler.duration();
                if (!howlerDuration) return;
                seekLockUntil = Date.now() + 500;
                closeToasterEditors();
                var pct = progressRange.value / 100;
                if (pct > 0.98) pct = 0.98;
                if (pct < 0.0) pct = 0.0;
                var newSeek = howlerDuration * pct;
                seekTarget = newSeek;
                howler.seek(newSeek);
                progressBar.style.width = (pct * 100) + '%';
                progressDot.style.marginLeft = (pct * 100) + '%';
                currentTime.innerHTML = formatTime(Math.round(newSeek));
                progressRange.setAttribute('aria-valuetext', formatTime(Math.round(newSeek)));
            });
            progressRange.addEventListener('keydown', function (event) {
                if (event.key !== ' ' && event.key !== 'Enter') return;
                if (!howler) return;
                var howlerDuration = howler.duration();
                if (!howlerDuration) return;
                event.preventDefault();
                event.stopPropagation();
                var now = Date.now();
                var seek;
                if (seekTarget !== null && now < seekLockUntil) {
                    seek = seekTarget;
                } else {
                    seek = howler.seek() || 0;
                }
                seekLockUntil = now + 500;
                var seekStep = 5;
                seek = event.shiftKey ? Math.max(0, seek - seekStep) : Math.min(howlerDuration * 0.98, seek + seekStep);
                seekTarget = seek;
                howler.seek(seek);
                var pct = seek / howlerDuration;
                progressBar.style.width = (pct * 100) + '%';
                progressDot.style.marginLeft = (pct * 100) + '%';
                progressRange.value = Math.round(pct * 100);
                currentTime.innerHTML = formatTime(Math.round(seek));
                progressRange.setAttribute('aria-valuetext', formatTime(Math.round(seek)));
            });
        }
        volumeOverlay.addEventListener("mousedown",
            function() {
                volumeSliderDown = true;
                const pct = 1 - (event.layerY / parseFloat(volumeOverlay.scrollHeight));
                adjustVolume(pct);
            });

        volumeOverlay.addEventListener("touchstart",
            function() {
                volumeSliderDown = true;
                const rect = event.target.getBoundingClientRect();
                const y = rect.bottom - event.targetTouches[0].pageY;
                const pct = y / parseFloat(volumeOverlay.scrollHeight);
                adjustVolume(pct);
            });

        volumeOverlay.addEventListener("mouseup",
            function() {
                if (volumeSliderDown) {
                    closeVolumeControl();
                }
                volumeSliderDown = false;
            });

        volumeOverlay.addEventListener("blur",
            function() {
                if (volumeSliderDown) {
                    closeVolumeControl();
                }
                volumeSliderDown = false;
            });

        volumeOverlay.addEventListener("touchend",
            function() {
                if (volumeSliderDown) {
                    closeVolumeControl();
                }
                volumeSliderDown = false;
            });

        volumeOverlay.addEventListener("mousemove",
            function() {
                if (volumeSliderDown) {
                    const pct = 1 - (event.layerY / parseFloat(volumeOverlay.scrollHeight));
                    adjustVolume(pct);
                }
            });

        volumeOverlay.addEventListener("touchmove",
            function() {
                if (volumeSliderDown) {
                    const rect = event.target.getBoundingClientRect();
                    const y = rect.bottom - event.targetTouches[0].pageY;
                    const pct = y / parseFloat(volumeOverlay.scrollHeight);
                    adjustVolume(pct);
                }
            });

        // Arrow key navigation for speed radio group
        if (speedControl) {
            speedControl.addEventListener('keydown', function (event) {
                var radios = Array.from(speedControl.querySelectorAll('[role="radio"]'));
                var current = document.activeElement;
                var idx = radios.indexOf(current);
                if (idx === -1) return;

                var handled = false;
                if (event.key === 'Enter' || event.key === ' ') {
                    event.preventDefault();
                    current.click();
                    window.speedControl.style.display = "none";
                    window.speedNav.focus();
                    return;
                } else if (event.key === 'ArrowRight' || event.key === 'ArrowDown') {
                    idx = (idx + 1) % radios.length;
                    handled = true;
                } else if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') {
                    idx = (idx - 1 + radios.length) % radios.length;
                    handled = true;
                } else if (event.key === 'Escape') {
                    window.speedControl.style.display = "none";
                    window.speedNav.focus();
                    event.preventDefault();
                    return;
                } else if (event.key === 'Tab') {
                    window.speedControl.style.display = "none";
                    return;
                }

                if (handled) {
                    event.preventDefault();
                    speedKeyNav = true;
                    radios[idx].focus();
                    radios[idx].click();
                    speedKeyNav = false;
                }
            });
        }

        // Native range input for volume slider - works with all screen readers
        if (volumeRange) {
            volumeRange.addEventListener('input', function () {
                adjustVolume(volumeRange.value / 100);
            });
            volumeRange.addEventListener('keydown', function (event) {
                if (event.key === ' ' || event.key === 'Enter') {
                    event.preventDefault();
                    event.stopPropagation();
                    var vol = howler ? howler.volume() : (volumeRange.value / 100);
                    vol = event.shiftKey ? Math.max(0, vol - 0.1) : Math.min(1.0, vol + 0.1);
                    adjustVolume(vol);
                } else if (event.key === 'Escape') {
                    closeVolumeControl();
                    event.preventDefault();
                } else if (event.key === 'Tab') {
                    closeVolumeControl();
                }
            });
        }

        // Ensure all buttons respond to Enter and Space keys for accessibility
        [playPauseBtn, speedNav, volumeNavBtn, audioCloseBtn, miniPlayBtn, miniPauseBtn, window.audioLauncher].forEach(function(btn) {
            if (btn) {
                btn.addEventListener('keydown', function(event) {
                    if (event.key === 'Enter' || event.key === ' ') {
                        event.preventDefault();
                        this.click();
                    }
                });
            }
        });
    }

    function adjustProgress(pct) {
        closeToasterEditors();

        if (pct > 0.98) pct = 0.98;
        if (pct < 0.0) pct = 0.0;
        const howlerDuration = howler.duration();
        const newSeek = howlerDuration * pct;
        howler.seek(newSeek);
    }

    function closeToasterEditors() {
        volumeEditor.style.display = "none";
        speedControl.style.display = "none";
    }

    function adjustVolume(pct) {
        if (pct < 0) pct = 0;
        if (pct > 1.0) pct = 1.0;
        if (pct <= 1.0) {
            howler.volume(pct);
            volumeBar.style.height = ((pct * 100) || 0) + "%";
            volumeDot.style.top = ((100 - (pct * 100)) || 0) + "%";
            if (volumeRange) volumeRange.value = Math.round(pct * 100);
        }
    }
    var speedKeyNav = false;
    function adjustRate(speedElement, rate) {
        playRate = rate;
        howler.rate(rate, soundId);
        window.speedNav.innerText = speedElement.innerText.trim();
        window.speedNav.setAttribute('aria-label', 'Change playback speed, currently ' + speedElement.innerText.trim());

        // Only close the panel and return focus when it was actually open
        if (!speedKeyNav) {
            if (window.speedControl.style.display === "flex") {
                window.speedControl.style.display = "none";
                window.speedNav.focus();
            }
        }

        // Sync radio group state
        var radios = window.speedControl.querySelectorAll('[role="radio"]');
        radios.forEach(function (r) {
            r.setAttribute('aria-checked', 'false');
            r.setAttribute('tabindex', '-1');
        });
        speedElement.setAttribute('aria-checked', 'true');
        speedElement.setAttribute('tabindex', '0');

        // Update visual styling
        var allSpeedSelections = document.querySelectorAll(".speed-selection");
        allSpeedSelections.forEach(function (el) {
            el.style.fontWeight = speedNavFontWeightOff;
            el.style.color = speedNavColorOff;
        });
        speedElement.style.fontWeight = speedNavFontWeightOn;
        speedElement.style.color = speedNavColorOn;
    }
    function displaySpeedNav() {
        if (speedControl.style.display == "flex") {
            speedControl.style.display = "none";
        } else {
            speedControl.style.display = "flex";
            volumeEditor.style.display = "none";
            // Focus the currently selected speed radio
            var checked = window.speedControl.querySelector('[aria-checked="true"]');
            if (checked) checked.focus();
        }

        const currentSpeed = speedNav.innerText.replace("x", "");
        for (let i = 0; i < speedSelectionElements.length; i++) {
            const elem = speedSelectionElements[i];
            if (parseFloat(elem.dataset.rate) == parseFloat(currentSpeed)) {
                elem.style.fontWeight = speedNavFontWeightOn;
                elem.style.color = speedNavColorOn;
            } else {
                elem.style.fontWeight = speedNavFontWeightOff;
                elem.style.color = speedNavColorOff;
            }
        }
    }

    var step = function () {

        const seek = howler.seek() || 0;

        //highlight and scroll
        readAlong(Math.floor(seek * 1000));

        // Skip visual updates while a keyboard seek is being processed
        if (Date.now() < seekLockUntil) {
            if (howler.playing()) {
                requestAnimationFrame(step.bind(window));
            }
            return;
        }
        seekTarget = null;

        //update the main player progress-bar
        var timeToDisplay = Math.round(seek);
        if (timerCountdown) {
            timeToDisplay = howler.duration() - Math.round(seek);
        }
        currentTime.innerHTML = formatTime(timeToDisplay);
        var progressPct = (((seek / howler.duration()) * 100) || 0);
        progressBar.style.width = progressPct + "%";
        progressDot.style.marginLeft = progressPct + "%";
        if (progressRange) progressRange.value = Math.round(progressPct);

        if (hasMiniPlayer) {
            updateMiniPlayerProgress(seek / howler.duration());
        }

        if (howler.playing()) {
            requestAnimationFrame(step.bind(window));
        }
    }
    function updateDisplay(state) {
        if (state === "disconnected") {

            if (loadBtn) loadBtn.style.display = "block";
            playBtn.style.display = "none";
            pauseBtn.style.display = "none";
            playPauseBtn.setAttribute('aria-label', 'Loading audio');

            if (miniPlayBtn) {
                miniPlayBtn.style.display = "none";
                miniPauseBtn.style.display = "none";
            }
        }
        else if (state === "stopped") {
            if (loadBtn) loadBtn.style.display = "none";
            playBtn.style.display = "block";
            pauseBtn.style.display = "none";
            playPauseBtn.setAttribute('aria-label', 'Play');

            if (howler && progressRange) {
                var seek = howler.seek() || 0;
                var pct = (((seek / howler.duration()) * 100) || 0);
                progressRange.value = Math.round(pct);
                progressRange.setAttribute('aria-valuetext', formatTime(Math.round(seek)));
            }

            if (miniPlayBtn) {
                miniPlayBtn.style.display = "block";
                miniPauseBtn.style.display = "none";
            }
        }
        else if (state === "playing") {
            if (loadBtn) loadBtn.style.display = "none";
            playBtn.style.display = "none";
            pauseBtn.style.display = "block";
            playPauseBtn.setAttribute('aria-label', 'Pause');

            if (miniPlayBtn) {
                miniPlayBtn.style.display = "none";
                miniPauseBtn.style.display = "block";
            }
        }
    }

    function showAudioNav() {

        if (audioPlayerContainer) {
            hasPlayed = false;

            //initialize as loading the sound file
            updateDisplay("disconnected");

            //RESPONSIVE
            if (audioPlayerContainer.offsetWidth < 500) {
                if (contentConfig.responsiveIncludePlayerTime && !contentConfig.responsiveIncludePlayerDuration) {
                    timerCountdown = true;
                }
                if (!contentConfig.responsiveIncludePlayerDuration) {
                    if (duration) {
                        duration.style.display = "none";
                    }
                    if (timeDivider) {
                        timeDivider.style.display = "none";
                    }
                }
                if (!contentConfig.responsiveIncludePlayerTime) {
                    if (currentTime) {
                        currentTime.style.display = "none";
                    }
                }
                if (!contentConfig.responsiveIncludePlayerRate) {
                    if (speedNav) {
                        speedNav.style.display = "none";
                    }
                }
                if (!contentConfig.responsiveIncludePlayerVolume) {
                    if (volumeNav) {
                        volumeNav.style.display = "none";
                    }
                }
                if (!contentConfig.responsiveIncludePlayerClose) {
                    if (audioClose) {
                        audioClose.style.display = "none";
                    }
                }
            }

            parent.audioSrc = audioPlayerContainer.getAttribute("read-along-audio-source");
            howler = new Howl({
                src: [parent.audioSrc],
                html5: true,
                onload: function () {
                    updateDisplay("stopped");
                    duration.innerHTML = formatTime(howler.duration());
                    if (progressRange) {
                        var stepPct = Math.max(1, Math.round((5 / howler.duration()) * 100));
                        progressRange.step = stepPct;
                    }
                    autoPlayAudio = "false";
                    play(); //play immediately
                },
                onplay: function () {
                    updateDisplay("playing");
                    lastListen = Date.now() / 1000;
                    duration.innerHTML = formatTime(howler.duration());
                    requestAnimationFrame(step.bind(window));
                    if (focusPauseOnPlay) {
                        playPauseBtn.focus();
                        focusPauseOnPlay = false;
                    }
                },
                onend: function() {
                    if (!progressSliderDown) {
                        updateDisplay("stopped");
                    }
                },
                onseek: function() {
                    lastListen = Date.now() / 1000; //probably don't need this???
                    requestAnimationFrame(step.bind(window));
                },
                onloaderror: function (id, error) {
                    updateDisplay("disconnected");
                    if (listening) {
                        setTimeout(showAudioNav, 3000); //retry in a few seconds
                    }
                },
                onplayerror: function (id, error) {
                    updateDisplay("stopped");
                    if (listening) {
                        setTimeout(play, 3000); //retry in a few seconds
                    }
                }
            });
            return;
        }
    }
    function activateAudioPlayer() {
        audioPlayer.style.display = "flex";
        navMain.style.display = "none";
        play();
        playPauseBtn.focus();
    }

    function play() {
        try {
            soundId = howler.play();
            if (!hasPlayed) {
                try {
                    const el = document.getElementsByClassName("defaultSpeed")[0];
                    adjustRate(el, el.dataset.rate);
                } catch (ex) {
                }
            }
            hasPlayed = true;
        } catch (e) {
        }
    }
    this.pause = function () {
        try {
            updateDisplay("stopped");
            howler.pause();
            playBtn.style.display = 'block';
            pauseBtn.style.display = 'none';
        } catch (e) {
        }
    }

    //window.addEventListener('online', () => {
    //    play();
    //});

    //window.addEventListener('offline', () => {
    //    updateDisplay("disconnected");
    //    howler.pause();
    //});
    

    function readAlong(audTime) {
        try {
            const nowTime = Date.now();
            if (!lastHighlight || ((nowTime - lastHighlight) < 200) || progressSliderDown) {
                return;
            }

            checkSendReadAlongEvent();

            if (audioOnly)
                return;

            lastHighlight = nowTime;

            const emagsPage = document.scrollingElement;
            if (!emagsPage) {
                return;
            }
            const elements = emagsPage.getElementsByClassName("readAlong");
            const elementCount = elements.length;

            let element = null;
            for (let i = 0; i < elementCount; ++i) {
                const e = elements[i];
                const audStart = e.getAttribute("audStart");
                const time = parseInt(audStart.substring(4, audStart.length));
                if (!highlightedElement) {
                    element = e;
                    break;
                } else if (time < audTime) {
                    element = e;
                } else {
                    break;
                }
            }
            if (element != null) {
                if (!highlightedElement || (highlightedElement && element !== highlightedElement)) {
                    if (highlightedElement) {
                        highlightedElement.style.backgroundColor = null;
                    }
                    if (autoScroll) {
                        //const elementOffsetTop = getOffsetTop(element);
                        //const emagsPageOffsetTop = getOffsetTop(emagsPage);
                        const scrollOffset = Math.max(scrollOffsetMin, screen.height * 0.25);
                        //var scrollTop = ((elementOffsetTop - emagsPageOffsetTop) - scrollOffset);
                        //Promise.resolve($(emagsPage).animate({scrollTop: scrollTop }, 500));
                        smoothScroll(element, scrollOffset);
                    }
                }
                highlightedElement = element;
                highlightedElement.style.backgroundColor = highlightColor;
            }
        } catch (ex) {
            audioOnly = true;
            sendContentError("");
        }
    }

    function smoothScroll(elm, scrollOffset) {
        const startY = currentYPosition();
        const stopY = elmYPosition(elm) - scrollOffset;
        const distance = stopY > startY ? stopY - startY : startY - stopY;
        if (distance < 50) {
            scrollTo(0, stopY);
            return;
        }
        var speed = Math.round(distance / 50);
        if (speed >= 20) speed = 20;
        const step = Math.round(distance / 25);
        var leapY = stopY > startY ? startY + step : startY - step;
        var timer = 10;
        if (stopY > startY) {
            for (var i = startY; i < stopY; i += step) {
                setTimeout(`window.scrollTo(0, ${leapY})`, timer * speed);
                leapY += step;
                if (leapY > stopY) leapY = stopY;
                timer++;
            }
            return;
        }
        for (var i = startY; i > stopY; i -= step) {
            setTimeout(`window.scrollTo(0, ${leapY})`, timer * speed);
            leapY -= step;
            if (leapY < stopY) leapY = stopY;
            timer++;
        }
    }

    function currentYPosition() {
        // Firefox, Chrome, Opera, Safari
        if (window.pageYOffset) return window.pageYOffset;
        // Internet Explorer 6 - standards mode
        if (document.documentElement && document.documentElement.scrollTop)
            return document.documentElement.scrollTop;
        // Internet Explorer 6, 7 and 8
        if (document.body.scrollTop) return document.body.scrollTop;
        return 0;
    }


    function elmYPosition(elm) {
        //var elm = document.getElementById(eID);
        var y = elm.offsetTop;
        var node = elm;
        while (node.offsetParent && node.offsetParent != document.body) {
            node = node.offsetParent;
            y += node.offsetTop;
        }
        return y;
    }

    function getOffsetTop(element) {
        let offsetTop = 0;
        while (element) {
            offsetTop += element.offsetTop;
            element = element.offsetParent;
        }
        return offsetTop;
    }

    function closeVolumeControl() {
        innerControls.style.display = "flex";
        volumeEditor.style.display = "none";
        volumeNavBtn.focus();
    }

    function clearAudio() {
        hasPlayed = false;
        parent.pause();
        listening = false;
        audioPlayerContainer.style.display = "none";
        volumeEditor.style.display = "none";
        innerControls.style.display = "flex";
        if (highlightedElement) {
            highlightedElement.style.background = "rgba(255,255,255,0)";
        }
        audioLauncherContainer.style.display = "flex";
        window.audioLauncher.focus();
        if (miniPlayer)
            miniPlayer.style.display = "none";
    }

    function disableAudioPlayer() {
        audioPlayerContainer.style.display = "none";
        navAudio.style.display = "block";
        navAudioActive.style.display = "none";
        parent.pause();
    }

    function toggleVolume() {
        var display = (volumeControl.style.display === "block") ? "none" : "block";

        setTimeout(function() {
                volumeControl.style.display = display;
            },
            (display === "block") ? 0 : 500);
        volumeControl.className = (display === "block") ? "fadein" : "fadeout";
    }

    var formatTime = function(secs) {
        secs = Math.round(secs);
        const minutes = Math.floor(secs / 60) || 0;
        const seconds = (secs - minutes * 60) || 0;
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
    }

    function formatTimeMinutes(secs) {
        const minutes = Math.round(secs / 60, 0);
        return minutes;
    }

    function pseudoGuid() {
        const s = [];
        const hexDigits = "0123456789abcdef";
        for (let i = 0; i < 36; ++i) {
            s[i] = hexDigits.substr(Math.floor(Math.random() * 0x10), 1);
        }
        s[14] = "4";
        s[19] = hexDigits.substr((s[19] & 0x3) | 0x8, 1);
        s[8] = s[13] = s[18] = s[23] = "-";

        const guid = s.join("");
        return guid;
    }
};

try {
    var readAlongScript = document.getElementById('readAlongScript');
    if (!readAlongScript) {
        readAlongScript = [...document.getElementsByTagName('script')].find(function(elem) {
            return (elem.src && elem.src.toLowerCase().includes(`${apiDomain}/readalong/`));
        });
    }
    if (readAlongScript) {
        readAlongScriptSrc = readAlongScript.src;
        readAlongScriptId = readAlongScriptSrc.substring(readAlongScriptSrc.lastIndexOf("/") + 1);
        if (readAlongScriptId) {
            readAlong = new ReadAlong(readAlongScriptId);
            players.push(readAlong);
            readAlong.start();
        }
    }
} catch (e) {
    console.warn(e);
}