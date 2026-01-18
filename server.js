const express = require("express");
const path = require("path");
const { spawn } = require("node:child_process");
const fs = require("node:fs");

const app = express();
const PORT = 3000;

app.use(express.json({ limit: "1mb" }));

// Serve frontend from /public using express.static (standard Express way) [web:82][web:98]
app.use(express.static(path.join(__dirname, "public")));

function splitCompilerOutput(stdoutText) {
  // We assume your C++ compiler prints these headers:
  // TOKENS:
  // SYMBOL TABLE:
  // INTERMEDIATE CODE (TAC):
  const out = stdoutText || "";

  const a = out.indexOf("TOKENS:");
  const b = out.indexOf("SYMBOL TABLE:");
  const c = out.indexOf("INTERMEDIATE CODE (TAC):");

  // If format is unexpected, fall back to raw
  if (a === -1 || b === -1 || c === -1) {
    return { tokens: "", symbolTable: "", tac: "", raw: out };
  }

  const tokens = out.slice(a, b).trimEnd() + "\n";
  const symbolTable = out.slice(b, c).trimEnd() + "\n";
  const tac = out.slice(c).trimEnd() + "\n";

  // For the tabs, usually users want only the body; but you asked to mirror exactly.
  // So we keep the headers included as your compiler printed them.
  return { tokens, symbolTable, tac, raw: out };
}

app.post("/api/compile", async (req, res) => {
  const code = req.body && req.body.code ? String(req.body.code) : "";

  // Optional: save last input for debugging/demo
  // fs.writeFileSync(path.join(__dirname, "last_input.txt"), code, "utf8");

  const exePath = path.join(__dirname, "bin", "compiler.exe");
  if (!fs.existsSync(exePath)) {
    return res.status(500).json({
      ok: false,
      exitCode: -1,
      stdout: "",
      stderr: `compiler.exe not found at: ${exePath}\nPut your compiled C++ compiler in mini-compiler-web/bin/compiler.exe`,
      tokens: "",
      symbolTable: "",
      tac: "",
    });
  }

  // Spawn compiler.exe and pipe stdin/stdout/stderr [web:78][web:96]
  const child = spawn(exePath, [], { stdio: ["pipe", "pipe", "pipe"] });

  let stdout = "";
  let stderr = "";

  child.stdout.setEncoding("utf8");
  child.stderr.setEncoding("utf8");

  child.stdout.on("data", (chunk) => {
    stdout += chunk;
  });
  child.stderr.on("data", (chunk) => {
    stderr += chunk;
  });

  child.on("error", (err) => {
    return res.status(500).json({
      ok: false,
      exitCode: -1,
      stdout,
      stderr: stderr + "\n" + String(err),
      tokens: "",
      symbolTable: "",
      tac: "",
    });
  });

  // Send the program to compiler stdin, then END so it exits. [web:78]
  child.stdin.write(code);
  child.stdin.end();

  child.on("close", (exitCode) => {
    const parts = splitCompilerOutput(stdout);
    res.json({
      ok: exitCode === 0,
      exitCode,
      stdout,
      stderr,
      tokens: parts.tokens,
      symbolTable: parts.symbolTable,
      tac: parts.tac,
    });
  });
});

app.listen(PORT, () => {
  console.log(`Mini Compiler Web IDE running at http://localhost:${PORT}`);
});
