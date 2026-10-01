R"WEBHEAD(<!DOCTYPE HTML>
<html lang="cs">
<head>
  <title>LD2450 Security</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    :root { --bg: #0a0a0a; --card: #161616; --text: #e0e0e0; --accent: #03dac6; --warn: #cf6679; --sec: #333; }
    body.light-mode { --bg: #f0f2f5; --card: #fff; --text: #333; --accent: #018786; --warn: #b00020; --sec: #ddd; }
    * { box-sizing: border-box; }
    body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 8px; padding-bottom: 20px; transition: background 0.3s, color 0.3s; }
    h2 { color: var(--accent); margin: 4px 0; font-size: 1.3rem; display: flex; align-items: center; justify-content: center; gap: 8px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 8px; max-width: 700px; margin: 0 auto; }
    .card { background: var(--card); padding: 10px; border-radius: 10px; box-shadow: 0 3px 8px rgba(0,0,0,0.5); transition: background 0.3s; }
    .cc { max-width: 550px; margin: 8px auto; }
    .stat-row { display: flex; justify-content: space-between; margin-bottom: 5px; font-size: 0.85rem; border-bottom: 1px solid #222; padding-bottom: 3px; }
    .stat-val { font-weight: bold; color: #fff; }
    .gauge { text-align: center; margin-bottom: 5px; }
    .big-val { font-size: 2rem; font-weight: bold; line-height: 1; }
    .unit { font-size: 0.8rem; color: #888; }
    .icon { width: 16px; height: 16px; display: inline-block; vertical-align: middle; border-radius: 50%; }
    .icon.ok { background: #00ff00; box-shadow: 0 0 5px #00ff00; }
    .icon.warn { background: orange; }
    .icon.err { background: #ff0000; }
    input[type=range] { width: 100%; accent-color: var(--accent); }
    input[type=text], input[type=password], input[type=number], select { background: #222; border: 1px solid #444; color: white; padding: 8px; border-radius: 4px; width: 100%; margin-top: 2px; }
    body.light-mode input[type=text], body.light-mode input[type=password], body.light-mode input[type=number], body.light-mode select { background: #f5f5f5; border-color: #ccc; color: #333; }
    .row-input { display: flex; justify-content: space-between; align-items: center; margin-bottom: 5px; gap: 10px; }
    button { width: 100%; padding: 8px; border: none; border-radius: 6px; background: #3700b3; color: white; cursor: pointer; margin-top: 4px; }
    button:hover { opacity: 0.9; }
    button.sec { background: var(--sec); }
    button.warn { background: var(--warn); color: black; font-weight: bold; }
    .tabs { display: flex; gap: 4px; margin-bottom: 10px; flex-wrap: wrap; }
    .tab { flex: 1; min-width: 65px; padding: 8px; background: #222; text-align: center; cursor: pointer; border-radius: 6px; font-size: 0.8rem; }
    .tab.active { background: var(--accent); color: black; font-weight: bold; }
    body.light-mode .tab { background: #e0e0e0; }
    body.light-mode .tab.active { background: var(--accent); color: white; }
    .hidden { display: none; }
    .section-title { color: #888; font-size: 0.8rem; margin: 10px 0 4px 0; text-transform: uppercase; border-bottom: 1px solid #333; }
    #canvasWrapper { position: relative; width: 100%; max-width: 400px; margin: 0 auto; }
    canvas { background: #000; border-radius: 8px; border: 2px solid #222; width: 100%; height: auto; }
    .legend { display: flex; justify-content: center; gap: 12px; margin-top: 8px; font-size: 0.7rem; flex-wrap: wrap; }
    .legend-item { display: flex; align-items: center; gap: 4px; }
    .legend-dot { width: 10px; height: 10px; border-radius: 50%; }
    .slider-container { text-align: left; margin: 5px 0; }
    .slider-container label { font-size: 0.85rem; color: #aaa; }
    #toast { position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%); background: #333; padding: 10px 20px; border-radius: 20px; opacity: 0; transition: opacity 0.3s; pointer-events: none; z-index: 10; }
    @media (max-width: 480px) {
        .grid { grid-template-columns: 1fr; gap: 8px; }
        body { padding: 5px; padding-bottom: 60px; }
        .big-val { font-size: 2rem; }
        .tabs { gap: 3px; }
        .tab { min-width: 50px; font-size: 0.7rem; padding: 8px 4px; }
        button { padding: 12px; min-height: 44px; font-size: 1rem; }
        .row-input { flex-direction: column; align-items: stretch; gap: 2px; }
        input[type=text], input[type=number], select { padding: 10px; font-size: 1rem; }
    }
  </style>
</head>
)WEBHEAD"
