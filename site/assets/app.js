(function () {
  "use strict";

  var root = document.documentElement;
  var button = document.getElementById("lang-toggle");

  function setLanguage(lang) {
    var isZh = lang === "zh";
    root.setAttribute("data-lang", isZh ? "zh" : "en");
    root.setAttribute("lang", isZh ? "zh-CN" : "en");
    if (button) {
      button.textContent = isZh ? "EN" : "中文";
      button.setAttribute("aria-label", isZh ? "Switch to English" : "切换到中文");
    }
    try {
      localStorage.setItem("nanox-lang", isZh ? "zh" : "en");
    } catch (error) {
      /* private mode: the choice just will not persist */
    }
  }

  var saved = null;
  try {
    saved = localStorage.getItem("nanox-lang");
  } catch (error) {
    saved = null;
  }

  // ?lang=zh / ?lang=en wins, so a link can pin the language.
  var requested = null;
  try {
    requested = new URLSearchParams(window.location.search).get("lang");
  } catch (error) {
    requested = null;
  }
  if (requested !== "zh" && requested !== "en") {
    requested = null;
  }

  var prefersZh = (navigator.language || "en").toLowerCase().indexOf("zh") === 0;
  setLanguage(requested || saved || (prefersZh ? "zh" : "en"));

  if (button) {
    button.addEventListener("click", function () {
      setLanguage(root.getAttribute("data-lang") === "zh" ? "en" : "zh");
    });
  }
})();
