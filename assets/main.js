// Mosa Prolog LP: OS 別の手順の切り替えと、コマンドのコピー。
// JS が動かなくても、全 OS の手順がラベル付きで読める。
(function () {
  "use strict";

  var root = document.documentElement;
  var isEn = (root.lang || "").indexOf("en") === 0;
  var text = isEn
    ? { copy: "Copy", copied: "Copied", failed: "Copy failed" }
    : { copy: "コピー", copied: "コピーしました", failed: "コピーできませんでした" };

  // ---------- OS の切り替え ----------
  var STORE_KEY = "mosa-lp-os";
  var OS_LIST = ["mac", "linux", "win"];

  function guessOS() {
    var p =
      (navigator.userAgentData && navigator.userAgentData.platform) ||
      navigator.platform ||
      "";
    var ua = navigator.userAgent || "";
    if (/win/i.test(p) || /Windows/.test(ua)) return "win";
    if ((/linux/i.test(p) || /Linux/.test(ua)) && !/Android/i.test(ua)) return "linux";
    return "mac";
  }

  function readStored() {
    try {
      var v = window.localStorage.getItem(STORE_KEY);
      return OS_LIST.indexOf(v) >= 0 ? v : null;
    } catch (e) {
      return null;
    }
  }

  function store(v) {
    try {
      window.localStorage.setItem(STORE_KEY, v);
    } catch (e) {
      /* 保存できなくても動く */
    }
  }

  var scope = document.querySelector("[data-os-scope]");
  var picker = document.querySelector(".os-pick");

  function setOS(os) {
    if (!scope) return;
    scope.setAttribute("data-current-os", os);
    if (!picker) return;
    var buttons = picker.querySelectorAll("button[data-os-value]");
    for (var i = 0; i < buttons.length; i++) {
      var on = buttons[i].getAttribute("data-os-value") === os;
      buttons[i].setAttribute("aria-pressed", on ? "true" : "false");
    }
  }

  if (scope && picker) {
    setOS(readStored() || guessOS());
    picker.addEventListener("click", function (ev) {
      var btn = ev.target.closest("button[data-os-value]");
      if (!btn) return;
      var os = btn.getAttribute("data-os-value");
      setOS(os);
      store(os);
    });
  }

  // ---------- コピー ----------
  function commandText(pre) {
    if (pre.hasAttribute("data-copy")) return pre.getAttribute("data-copy");
    var raw = pre.textContent || "";
    if (pre.classList.contains("console")) {
      return raw
        .split("\n")
        .filter(function (line) {
          return line.indexOf("$ ") === 0;
        })
        .map(function (line) {
          return line.slice(2);
        })
        .join("\n");
    }
    return raw.replace(/\n+$/, "");
  }

  if (navigator.clipboard && window.isSecureContext) {
    var blocks = document.querySelectorAll(".code.copyable");
    for (var i = 0; i < blocks.length; i++) {
      (function (block) {
        var head = block.querySelector(".code-head");
        var pre = block.querySelector("pre");
        if (!head || !pre) return;
        var btn = document.createElement("button");
        btn.type = "button";
        btn.className = "copy";
        btn.textContent = text.copy;
        var status = document.createElement("span");
        status.className = "visually-hidden";
        status.setAttribute("aria-live", "polite");
        head.appendChild(btn);
        head.appendChild(status);
        var timer = null;
        btn.addEventListener("click", function () {
          navigator.clipboard.writeText(commandText(pre)).then(
            function () {
              btn.textContent = text.copied;
              status.textContent = text.copied;
              clearTimeout(timer);
              timer = setTimeout(function () {
                btn.textContent = text.copy;
                status.textContent = "";
              }, 1800);
            },
            function () {
              btn.textContent = text.failed;
              status.textContent = text.failed;
            }
          );
        });
      })(blocks[i]);
    }
  }
})();
