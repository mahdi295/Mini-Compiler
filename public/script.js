const codeEl = document.getElementById("code");
const runBtn = document.getElementById("runBtn");
const loadSampleBtn = document.getElementById("loadSampleBtn");
const statusEl = document.getElementById("status");
const exitInfoEl = document.getElementById("exitInfo");

const panels = {
  tokens: document.getElementById("panel-tokens"),
  symbols: document.getElementById("panel-symbols"),
  tac: document.getElementById("panel-tac"),
  raw: document.getElementById("panel-raw"),
};

function setActiveTab(tabName) {
  document.querySelectorAll(".panel").forEach((p) => p.classList.add("hidden"));
  panels[tabName].classList.remove("hidden");

  document.querySelectorAll(".tab").forEach((b) => {
    const active = b.dataset.tab === tabName;
    b.classList.toggle("bg-indigo-600", active);
    b.classList.toggle("border-indigo-500", active);
  });
}

document.querySelectorAll(".tab").forEach((btn) => {
  btn.addEventListener("click", () => setActiveTab(btn.dataset.tab));
});

setActiveTab("tokens");

loadSampleBtn.addEventListener("click", () => {
  codeEl.value = `int a;
int b;

a = 5;
b = a + 10 * (2 - 1);
print b;
`;
});

async function runCompiler() {
  const src = codeEl.value;

  // UI state
  runBtn.disabled = true;
  runBtn.classList.add("opacity-60", "cursor-not-allowed");
  statusEl.textContent = "Running...";
  exitInfoEl.textContent = "";

  // Clear outputs
  panels.tokens.textContent = "";
  panels.symbols.textContent = "";
  panels.tac.textContent = "";
  panels.raw.textContent = "";

  try {
    const res = await fetch("/api/compile", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ code: src }),
    });

    const data = await res.json();

    panels.tokens.textContent = data.tokens || "";
    panels.symbols.textContent = data.symbolTable || "";
    panels.tac.textContent = data.tac || "";
    panels.raw.textContent =
      (data.stdout || "") + (data.stderr ? "\n" + data.stderr : "");

    exitInfoEl.textContent = `exit=${data.exitCode}`;
    statusEl.textContent = data.ok ? "Done" : "Error";

    // Auto-switch to Raw if error
    if (!data.ok) setActiveTab("raw");
  } catch (err) {
    statusEl.textContent = "Network error";
    panels.raw.textContent = String(err);
    setActiveTab("raw");
  } finally {
    runBtn.disabled = false;
    runBtn.classList.remove("opacity-60", "cursor-not-allowed");
  }
}

runBtn.addEventListener("click", runCompiler);
