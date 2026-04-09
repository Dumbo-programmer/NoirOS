---
layout: default
title: NoirOS Playground
---

# NoirOS Playground

Try a browser-based NoirOS terminal simulation. You can explore files, run shell-like commands, and test game/app stubs without setting up QEMU.

<div class="playground-wrap">
  <div class="playground-toolbar">
    <button class="playground-btn" id="pg-boot" type="button">Boot Demo</button>
    <button class="playground-btn" id="pg-tour" type="button">Run Tour</button>
    <button class="playground-btn" id="pg-reset" type="button">Reset Session</button>
  </div>

  <div class="playground-terminal" id="playground-terminal" role="region" aria-label="NoirOS Playground Terminal">
    <div class="playground-output" id="playground-output"></div>
    <div class="playground-input-row">
      <span class="playground-shell-prompt" id="playground-prompt">root@noiros:/$</span>
      <input class="playground-input" id="playground-input" type="text" autocomplete="off" spellcheck="false" aria-label="Command input">
    </div>
  </div>
</div>

<p class="playground-note">Try: <code>help</code>, <code>ls</code>, <code>cd docs</code>, <code>cat README.txt</code>, <code>run snake</code>, <code>tree</code>, <code>clear</code></p>

<script>
(function () {
  var outputEl = document.getElementById("playground-output");
  var inputEl = document.getElementById("playground-input");
  var promptEl = document.getElementById("playground-prompt");
  var bootBtn = document.getElementById("pg-boot");
  var tourBtn = document.getElementById("pg-tour");
  var resetBtn = document.getElementById("pg-reset");
  var terminalEl = document.getElementById("playground-terminal");

  var fsRoot;
  var cwd;
  var history = [];
  var historyIndex = -1;

  function fileNode(content) {
    return { type: "file", content: content };
  }

  function dirNode(children) {
    return { type: "dir", children: children || {} };
  }

  function buildFs() {
    return dirNode({
      "README.txt": fileNode("NoirOS Playground\\nA browser demo of shell and filesystem behavior."),
      "help.txt": fileNode("Type help to list commands."),
      "docs": dirNode({
        "kernel.md": fileNode("Kernel: interrupt handling, memory setup, runtime services."),
        "fs.md": fileNode("Filesystem: tree-based in-memory structure with safe bounds checks."),
        "editor.md": fileNode("Editor: scrolling text editor with save and read-only behavior.")
      }),
      "games": dirNode({
        "snake.game": fileNode("Snake app binary placeholder"),
        "pong.game": fileNode("Pong app binary placeholder"),
        "dodge.game": fileNode("Dodge app binary placeholder")
      }),
      "apps": dirNode({
        "shell.exe": fileNode("Noir shell runtime"),
        "editor.exe": fileNode("Noir text editor")
      })
    });
  }

  function cloneSegments(parts) {
    return parts.slice();
  }

  function cwdToPath() {
    return cwd.length ? "/" + cwd.join("/") : "/";
  }

  function normalizePath(rawPath) {
    if (!rawPath || rawPath === ".") return cloneSegments(cwd);
    var absolute = rawPath.charAt(0) === "/";
    var parts = rawPath.split("/");
    var out = absolute ? [] : cloneSegments(cwd);

    for (var i = 0; i < parts.length; i++) {
      var seg = parts[i];
      if (!seg || seg === ".") continue;
      if (seg === "..") {
        if (out.length) out.pop();
      } else {
        out.push(seg);
      }
    }
    return out;
  }

  function getNode(pathSegments) {
    var node = fsRoot;
    for (var i = 0; i < pathSegments.length; i++) {
      if (!node || node.type !== "dir") return null;
      node = node.children[pathSegments[i]];
    }
    return node || null;
  }

  function updatePrompt() {
    promptEl.textContent = "root@noiros:" + cwdToPath() + "$";
  }

  function appendLine(text, cls) {
    var div = document.createElement("div");
    div.className = "playground-line" + (cls ? " " + cls : "");
    div.textContent = text;
    outputEl.appendChild(div);
    terminalEl.scrollTop = terminalEl.scrollHeight;
  }

  function appendMultiline(text, cls) {
    var lines = String(text).split("\n");
    for (var i = 0; i < lines.length; i++) appendLine(lines[i], cls);
  }

  function listDir(node) {
    if (!node || node.type !== "dir") return [];
    var keys = Object.keys(node.children).sort();
    var out = [];
    for (var i = 0; i < keys.length; i++) {
      var name = keys[i];
      var child = node.children[name];
      out.push(child.type === "dir" ? name + "/" : name);
    }
    return out;
  }

  function renderTree(node, prefix) {
    if (!node || node.type !== "dir") return;
    var names = Object.keys(node.children).sort();
    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      var child = node.children[name];
      var isLast = i === names.length - 1;
      var branch = isLast ? "`-- " : "|-- ";
      appendLine(prefix + branch + name + (child.type === "dir" ? "/" : ""));
      if (child.type === "dir") {
        renderTree(child, prefix + (isLast ? "    " : "|   "));
      }
    }
  }

  function fakeBoot() {
    var bootLines = [
      "[BOOT] Multiboot header found",
      "[BOOT] Entering protected mode",
      "[INIT] IDT loaded, IRQ handlers online",
      "[INIT] Keyboard + mouse ready",
      "[INIT] Filesystem mounted",
      "[UI  ] NoirOS desktop renderer active",
      "[OK  ] System ready"
    ];
    for (var i = 0; i < bootLines.length; i++) {
      appendLine(bootLines[i], "info");
    }
  }

  function runGame(name) {
    var samples = {
      snake: ["Launching Snake...", "Score: 28", "Score: 44", "GAME OVER - New High Score: 44"],
      pong: ["Launching Pong...", "Ball speed up!", "Lives: 2", "Final Score: 120"],
      dodge: ["Launching Dodge...", "Wave 1 cleared", "Wave 2 cleared", "Hit limit reached"]
    };
    var lines = samples[name];
    if (!lines) {
      appendLine("Unknown game: " + name, "error");
      return;
    }
    for (var i = 0; i < lines.length; i++) appendLine(lines[i]);
  }

  function showHelp() {
    appendMultiline(
      "Commands:\n" +
      "  help                 Show this help\n" +
      "  ls [path]            List directory\n" +
      "  cd <path>            Change directory\n" +
      "  pwd                  Print current path\n" +
      "  cat <file>           Show file contents\n" +
      "  tree                 Show directory tree\n" +
      "  uname                System name\n" +
      "  whoami               Current user\n" +
      "  run <snake|pong|dodge>  Run game demo\n" +
      "  clear                Clear terminal\n" +
      "  boot                 Replay boot demo\n" +
      "  about                About NoirOS",
      "info"
    );
  }

  function executeCommand(line) {
    var trimmed = line.trim();
    if (!trimmed) return;

    appendLine(promptEl.textContent + " " + trimmed, "prompt");

    var parts = trimmed.split(/\s+/);
    var cmd = parts[0].toLowerCase();
    var arg = parts.length > 1 ? trimmed.slice(parts[0].length).trim() : "";

    if (cmd === "help") {
      showHelp();
      return;
    }

    if (cmd === "clear") {
      outputEl.innerHTML = "";
      return;
    }

    if (cmd === "boot") {
      fakeBoot();
      return;
    }

    if (cmd === "about") {
      appendLine("NoirOS x86 educational OS playground (browser simulation).", "info");
      return;
    }

    if (cmd === "uname") {
      appendLine("NoirOS i386 0.3-playground");
      return;
    }

    if (cmd === "whoami") {
      appendLine("root");
      return;
    }

    if (cmd === "pwd") {
      appendLine(cwdToPath());
      return;
    }

    if (cmd === "ls") {
      var lsPath = arg ? normalizePath(arg) : cloneSegments(cwd);
      var lsNode = getNode(lsPath);
      if (!lsNode || lsNode.type !== "dir") {
        appendLine("ls: not a directory", "error");
        return;
      }
      var entries = listDir(lsNode);
      appendLine(entries.length ? entries.join("  ") : "(empty)");
      return;
    }

    if (cmd === "cd") {
      if (!arg) {
        cwd = [];
        updatePrompt();
        return;
      }
      var cdPath = normalizePath(arg);
      var cdNode = getNode(cdPath);
      if (!cdNode || cdNode.type !== "dir") {
        appendLine("cd: no such directory", "error");
        return;
      }
      cwd = cdPath;
      updatePrompt();
      return;
    }

    if (cmd === "cat") {
      if (!arg) {
        appendLine("cat: missing file operand", "error");
        return;
      }
      var catPath = normalizePath(arg);
      var catNode = getNode(catPath);
      if (!catNode || catNode.type !== "file") {
        appendLine("cat: file not found", "error");
        return;
      }
      appendMultiline(catNode.content);
      return;
    }

    if (cmd === "tree") {
      appendLine("/", "info");
      renderTree(fsRoot, "");
      return;
    }

    if (cmd === "run") {
      if (!arg) {
        appendLine("run: expected app name", "error");
        return;
      }
      runGame(arg.toLowerCase());
      return;
    }

    appendLine(cmd + ": command not found", "error");
  }

  function bootWelcome() {
    appendLine("NoirOS Browser Playground", "info");
    appendLine("Type 'help' to get started.", "info");
    appendLine("", "info");
  }

  function resetSession() {
    fsRoot = buildFs();
    cwd = [];
    history = [];
    historyIndex = -1;
    outputEl.innerHTML = "";
    updatePrompt();
    bootWelcome();
    inputEl.value = "";
    inputEl.focus();
  }

  function runTour() {
    var cmds = ["uname", "ls", "cd docs", "ls", "cat kernel.md", "cd /", "run snake"];
    for (var i = 0; i < cmds.length; i++) executeCommand(cmds[i]);
  }

  inputEl.addEventListener("keydown", function (ev) {
    if (ev.key === "Enter") {
      var cmd = inputEl.value;
      if (cmd.trim()) {
        history.push(cmd);
        historyIndex = history.length;
      }
      executeCommand(cmd);
      inputEl.value = "";
      ev.preventDefault();
      return;
    }

    if (ev.key === "ArrowUp") {
      if (!history.length) return;
      historyIndex = Math.max(0, historyIndex - 1);
      inputEl.value = history[historyIndex] || "";
      ev.preventDefault();
      return;
    }

    if (ev.key === "ArrowDown") {
      if (!history.length) return;
      historyIndex = Math.min(history.length, historyIndex + 1);
      inputEl.value = historyIndex < history.length ? history[historyIndex] : "";
      ev.preventDefault();
    }
  });

  bootBtn.addEventListener("click", function () {
    executeCommand("boot");
    inputEl.focus();
  });

  tourBtn.addEventListener("click", function () {
    runTour();
    inputEl.focus();
  });

  resetBtn.addEventListener("click", function () {
    resetSession();
  });

  resetSession();
})();
</script>
