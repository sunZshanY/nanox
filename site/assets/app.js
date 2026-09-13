(function () {
  "use strict";

  var root = document.documentElement;
  var button = document.getElementById("lang-toggle");

  // The inline script in <head> already resolved the language before first
  // paint; this file only keeps the toggle in sync and persists choices.
  function syncButton() {
    var isZh = root.getAttribute("data-lang") === "zh";
    if (button) {
      button.textContent = isZh ? "EN" : "中文";
      button.setAttribute("aria-label", isZh ? "Switch to English" : "切换到中文");
    }
  }

  function setLanguage(lang) {
    var isZh = lang === "zh";
    root.setAttribute("data-lang", isZh ? "zh" : "en");
    root.setAttribute("lang", isZh ? "zh-CN" : "en");
    syncButton();
    try {
      localStorage.setItem("nanox-lang", isZh ? "zh" : "en");
    } catch (error) {
      /* private mode: the choice just will not persist */
    }
  }

  syncButton();

  if (button) {
    button.addEventListener("click", function () {
      setLanguage(root.getAttribute("data-lang") === "zh" ? "en" : "zh");
    });
  }
})();
