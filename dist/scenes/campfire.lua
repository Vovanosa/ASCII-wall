-- Winter campfire scene.
-- Engine APIs:
--   put(x, y, ch [, fg [, bg]])
--   text(x, y, str [, fg [, bg]])
--   fill(x, y, w, h, ch [, fg [, bg]])
--   clear([bg])
--   scene_value(name [, default])
-- update(t, dt, input) receives mouse position/speed and button state in cell coordinates.
-- Engine/render frame rate is independent from every animation speed below.

font = { name = "Consolas", size = 14 }
background = 0x070a14

local rand, floor, ceil, abs, sin, cos, sqrt, exp =
  math.random, math.floor, math.ceil, math.abs, math.sin, math.cos, math.sqrt, math.exp
local min, max = math.min, math.max
local LIGHT_COLORS = { 0xffd34e, 0xff5148, 0x55aaff, 0x55ed69 }
local MAX_INTERACTION_FLAKES = 160

local function clamp(v, lo, hi)
  return v < lo and lo or (v > hi and hi or v)
end

local function cfg(name, default)
  local v = scene_value(name, default)
  return v == v and v or default
end

local function mix(a, b, k)
  local ar, ag, ab = (a >> 16) & 255, (a >> 8) & 255, a & 255
  local br, bgc, bb = (b >> 16) & 255, (b >> 8) & 255, b & 255
  k = clamp(k, 0, 1)
  return (floor(ar + (br - ar) * k + 0.5) << 16)
       | (floor(ag + (bgc - ag) * k + 0.5) << 8)
       |  floor(ab + (bb - ab) * k + 0.5)
end

local RAMP = " .,:;+*#%@"
local BANDS = { 0x4a0c02, 0x8a1c04, 0xc23306, 0xe8550c,
                0xff8420, 0xffad3a, 0xffd36e, 0xfff3c8 }
local fireCh, fireCol = {}, {}
local MAXH = 36
for h = 0, MAXH do
  local k = h / MAXH
  local i = floor(k * (#RAMP - 1)) + 1
  fireCh[h], fireCol[h] = RAMP:sub(i, i),
    BANDS[min(#BANDS, floor(k * #BANDS) + 1)]
end

local LAYER_CH = { ".", "·", "*" }
local LAYER_COL = { 0x4b5672, 0x98a5c2, 0xf2f6ff }
local LAYER_V = { 2.5, 4.5, 7.5 }

local GROUND_BG, GROUND_FG = {}, {}
for l = 0, 6 do
  GROUND_BG[l] = mix(0x283146, 0xa0582a, l / 6 * 0.7)
  GROUND_FG[l] = mix(GROUND_BG[l], 0xe6edf8, 0.35)
end

local W, H, GY, FW, FH, FX, FY, DECAY
local heat, stars, flakes, sparks, coals, trees = {}, {}, {}, {}, {}, {}
local groundCh, groundD = {}, {}
local groundTraces = {}
local fireClock, glow, glowTimer = 0, 1, 0
local coalTimer = 0
local baseFlakeCount = 0
local previousMouseX, previousMouseY
local fireGust = { x = 0, y = 0, dx = 0, dy = 0, strength = 0 }
local settings

local function resetFlake(f, anywhere)
  local l = rand(1, 3)
  f.l, f.x = l, rand() * W
  f.y = anywhere and rand() * GY or -rand() * 4
  f.v = LAYER_V[l] * settings.snowSpeed * (0.8 + rand() * 0.4)
  f.ph, f.sw = rand() * 6.28, 0.6 + rand()
  f.held, f.extra = false, false
  f.holdX, f.holdY = nil, nil
  f.kickX, f.kickY, f.kickTime = 0, 0, 0
  return f
end

local function stepFire()
  local power = clamp(settings.firePower, 0, 3)
  local base = (FH - 1) * FW
  for x = 0, FW - 1 do
    local p = (x - (FW - 1) / 2) / (FW / 2)
    local left = 0.82 * exp(-((p + 0.68) / 0.16) ^ 2)
    local center = exp(-(p / 0.19) ^ 2)
    local right = 0.76 * exp(-((p - 0.66) / 0.16) ^ 2)
    local profile = max(left, center, right)
    local heightBonus = power * 12 * profile
    heat[base + x] = rand() < profile * 0.82
      and clamp(MAXH - rand(0, 4) + heightBonus, 0, MAXH)
      or rand(0, 5)
  end
  for y = 1, FH - 1 do
    local row = y * FW
    for x = 0, FW - 1 do
      local nx = x + rand(-1, 1)
      if nx < 0 then nx = 0 elseif nx >= FW then nx = FW - 1 end
      local nh = heat[row + x] - rand(0, DECAY)
      heat[row - FW + nx] = nh > 0 and nh or 0
    end
  end
end

local function makeTree(x, scale)
  local h = floor(6 + scale * 5)
  local halfWidth = max(2, floor(h * 0.65))
  local width = halfWidth * 2 + 1
  local snowRows = settings.treeSnow > 0
    and max(1, floor(h * settings.treeSnow + 0.5)) or 0
  return {
    x = x - halfWidth,
    h = h,
    width = width,
    halfWidth = halfWidth,
    snowRows = snowRows,
    maxSnowRows = snowRows,
    snowRegenTimer = 0,
    lit = false,
    lights = {}
  }
end

local function addTreeLights(tree)
  tree.lit = true
  local index = 0
  for row = 2, tree.h - 2 do
    local half = min(tree.halfWidth, ceil(row * tree.halfWidth / (tree.h - 1)))
    local first = row % 2 == 0 and -half + 1 or half - 1
    local last = -first
    local step = row % 2 == 0 and 3 or -3
    for offset = first, last, step do
      index = index + 1
      tree.lights[#tree.lights + 1] = {
        row = row,
        offset = offset,
        color = LIGHT_COLORS[(index - 1) % #LIGHT_COLORS + 1],
        phase = index % 2 == 0 and settings.treeLightInterval * 0.5 or 0
      }
    end
  end
end

local function drawTree(tree, top, t)
  local cx = tree.x + floor(tree.width / 2)
  local halfWidth = tree.halfWidth

  for row = 0, tree.h - 1 do
    local half = row == 0 and 0
      or min(halfWidth, ceil(row * halfWidth / (tree.h - 1)))
    local yy = top + row
    if yy >= 0 and yy < GY then
      if row == 0 then
        local color = tree.snowRows > 0 and 0xdce6f5 or 0x1f4d3a
        put(cx, yy, "^", color)
      else
        local snow = row < tree.snowRows
        for xx = cx - half, cx + half do
          if xx >= 0 and xx < W then
            local edge = xx == cx - half or xx == cx + half
            local ch = edge and (xx < cx and "/" or "\\") or "^"
            local color = snow and 0xdce6f5 or 0x1f4d3a
            put(xx, yy, ch, color)
          end
        end
      end
    end
  end

  for _, light in ipairs(tree.lights) do
    local cycle = floor((t + light.phase) / settings.treeLightInterval)
    if cycle % 2 == 0 then
      put(cx + light.offset, top + light.row, "+", light.color)
    end
  end

  local trunkY = top + tree.h
  local trunkWidth = tree.width >= 9 and 3 or 1
  for row = 0, 1 do
    for xx = cx - floor(trunkWidth / 2), cx + floor(trunkWidth / 2) do
      put(xx, trunkY + row, trunkWidth == 3 and "|" or "I", 0x4a3020)
    end
  end
end

function init()
  W, H = COLS, VIS_ROWS
  GY = H - max(4, floor(H * 0.12))
  settings = {
    starsDensity = clamp(cfg("stars_density", 1), 0, 5),
    starsSpeed = clamp(cfg("stars_speed", 1), 0, 10),
    starsVariety = clamp(cfg("stars_variety", 1), 0, 1),
    snowDensity = clamp(cfg("snow_density", 1), 0, 5),
    snowSpeed = clamp(cfg("snow_speed", 1), 0, 10),
    snowDrift = clamp(cfg("snow_drift", 1), 0, 5),
    wind = clamp(cfg("wind", 0), -20, 20),
    firePower = clamp(cfg("fire_power", 1), 0, 3),
    fireSpeed = clamp(cfg("fire_speed", 1), 0, 5) / 4,
    fireWidth = clamp(cfg("fire_width", 1), 0.25, 3),
    mouseFirePower = clamp(cfg("mouse_fire_power", 1), 0, 3),
    mouseSnowRadius = clamp(cfg("mouse_snow_radius", 7), 1, 12),
    mouseSnowPull = clamp(cfg("mouse_snow_pull", 1), 0.1, 3),
    mouseSnowLimit = floor(clamp(cfg("mouse_snow_limit", 36), 1, 72) + 0.5),
    treeShakePower = clamp(cfg("tree_shake_power", 1), 0, 3),
    treeCount = floor(clamp(cfg("tree_count", 10), 0, 20) + 0.5),
    treeJitter = clamp(cfg("tree_jitter", 0.25), 0, 0.45),
    treeLightCount = floor(clamp(cfg("tree_light_count", 4), 0, 20) + 0.5),
    treeLightsEnabled = cfg("tree_lights", 1) > 0,
    treeMin = clamp(cfg("tree_min_scale", 0.75), 0.35, 3),
    treeMax = max(clamp(cfg("tree_max_scale", 1.9), 0.35, 4),
                  clamp(cfg("tree_min_scale", 0.75), 0.35, 3)),
    treeSnow = clamp(cfg("tree_snow", 0.15), 0, 1),
    treeLightInterval = clamp(cfg("tree_light_interval", 1.5), 0.25, 30),
    groundTraceSeconds = clamp(cfg("ground_trace_seconds", 10), 1, 60),
    groundTracePermanent = cfg("ground_trace_permanent", 0) > 0,
    coalRate = max(0, cfg("coal_rate", 0.35)),
    glowPower = clamp(cfg("glow_power", 1), 0, 3)
  }

  FW = max(14, min(max(14, W - 2), floor(W * 0.11 * settings.fireWidth)))
  FH = max(10, min(24, floor(H * 0.28 * (0.7 + settings.firePower * 0.30))))
  FX, FY = floor((W - FW) / 2), GY - 1 - FH
  DECAY = max(2, floor(2 * MAXH / (0.9 * FH) + 0.5))
  for i = 0, FW * FH - 1 do heat[i] = 0 end

  stars = {}
  local starCount = floor(W * GY / 90 * settings.starsDensity)
  for i = 1, starCount do
    local target = rand(1, 3) == 1 and 0x7daaff
      or rand(1, 2) == 1 and 0xffb77d or 0xffedc4
    local tint = mix(0xc3d3ee, target, settings.starsVariety)
    stars[i] = {
      x = rand(0, W - 1),
      y = rand(0, max(0, GY - 8)),
      color = mix(background, tint, 0.72),
      bright = mix(tint, 0xffffff, 0.35),
      ph = rand() * 6.28,
      speed = 0.45 + rand() * 0.75,
      twinkle = rand() < 0.55
    }
  end

  flakes = {}
  local flakeCount = floor(W * GY / 60 * settings.snowDensity)
  for i = 1, flakeCount do flakes[i] = resetFlake({}, true) end
  baseFlakeCount = #flakes

  trees = {}
  if settings.treeCount > 0 then
    local maxTreeH = floor(6 + settings.treeMax * 5)
    local maxTreeHalf = max(2, floor(maxTreeH * 0.65))
    local leftFirst, leftLast = maxTreeHalf + 2, FX - 8
    local rightFirst, rightLast = FX + FW + 8, W - maxTreeHalf - 2
    local leftLength = max(0, leftLast - leftFirst)
    local rightLength = max(0, rightLast - rightFirst)
    local totalLength = leftLength + rightLength
    local leftCount = totalLength > 0
      and floor(settings.treeCount * leftLength / totalLength + 0.5) or 0
    leftCount = min(settings.treeCount, leftCount)
    local rightCount = settings.treeCount - leftCount
    if leftLength < 4 then leftCount, rightCount = 0, settings.treeCount end
    if rightLength < 4 then rightCount, leftCount = 0, settings.treeCount end

    local function fillSegment(first, last, count)
      if count <= 0 or last <= first then return end
      local spacing = count > 1 and (last - first) / (count - 1) or 0
      for i = 1, count do
        local center = count == 1 and (first + last) * 0.5 or first + (i - 1) * spacing
        center = center + (rand() * 2 - 1) * spacing * settings.treeJitter
        center = floor(clamp(center, first, last))
        local scale = settings.treeMin + rand() * (settings.treeMax - settings.treeMin)
        trees[#trees + 1] = makeTree(center, scale)
      end
    end
    fillSegment(leftFirst, leftLast, leftCount)
    fillSegment(rightFirst, rightLast, rightCount)
  end
  if settings.treeLightsEnabled then
    local lightCount = min(settings.treeLightCount, #trees)
    for i = 1, lightCount do
      local index = floor(i * (#trees + 1) / (lightCount + 1) + 0.5)
      addTreeLights(trees[index])
    end
  end

  local cx = FX + FW / 2
  groundCh, groundD = {}, {}
  for y = GY, ROWS - 1 do
    for x = 0, W - 1 do
      local i, r = (y - GY) * W + x, rand()
      groundCh[i] = r < 0.05 and "." or r < 0.08 and "'" or r < 0.10 and "~" or " "
      groundD[i] = sqrt((x - cx) ^ 2 + (2 * (y - GY + 1)) ^ 2) / (FW * 1.8)
    end
  end
  sparks, coals = {}, {}
  groundTraces = {}
  previousMouseX, previousMouseY = nil, nil
  fireGust = { x = 0, y = 0, dx = 0, dy = 0, strength = 0 }
  fireClock, glowTimer, coalTimer = 0, 0, 0
end

local function add_falling_snow(x, y, count, halfWidth, burst)
  local limit = baseFlakeCount + MAX_INTERACTION_FLAKES
  for _ = 1, count do
    if #flakes >= limit then break end
    local l = rand(1, 3)
    local spread = max(1, halfWidth)
    flakes[#flakes + 1] = {
      x = clamp(x + (rand() * 2 - 1) * spread, 0, W - 1),
      y = y + rand() * (burst and max(1, spread * 0.8) or 0.4),
      l = l,
      v = LAYER_V[l] * settings.snowSpeed * (0.8 + rand() * 0.4),
      ph = rand() * 6.28,
      sw = 0.6 + rand(),
      extra = true,
      life = 8,
      kickX = (rand() * 2 - 1) * (burst and (3 + spread * 0.65) or 1.5),
      kickY = burst and -(1.5 + rand() * 2.5) or 0,
      kickTime = burst and 0.9 or 0.35
    }
  end
end

local function tree_at(x, y)
  for i = #trees, 1, -1 do
    local tree = trees[i]
    local top = GY - tree.h - 2
    if x >= tree.x and x < tree.x + tree.width
      and y >= top and y < top + tree.h then
      return tree, top
    end
  end
end

local function shed_tree_snow(tree, top)
  local row = tree.snowRows - 1
  local half = row == 0 and 0
    or min(tree.halfWidth, ceil(row * tree.halfWidth / (tree.h - 1)))
  local center = tree.x + floor(tree.width / 2)
  tree.snowRows = row
  tree.snowRegenTimer = 0
  local count = floor(min(72, (42 + tree.h * 2) * settings.treeShakePower) + 0.5)
  if count > 0 then
    add_falling_snow(center, top + row, count, max(half, tree.halfWidth), true)
  end
end

local function handle_mouse_press(input)
  local tree, top = tree_at(input.x, input.y)
  if tree and tree.snowRows > 0 then
    shed_tree_snow(tree, top)
  end
end

local function update_tree_snow(dt)
  if settings.snowDensity <= 0 or settings.snowSpeed <= 0 then return end
  local accumulation = min(2, settings.snowDensity) * min(2, settings.snowSpeed)
  for _, tree in ipairs(trees) do
    if tree.snowRows < tree.maxSnowRows then
      tree.snowRegenTimer = tree.snowRegenTimer + dt * accumulation
      while tree.snowRegenTimer >= 35 and tree.snowRows < tree.maxSnowRows do
        tree.snowRows = tree.snowRows + 1
        tree.snowRegenTimer = tree.snowRegenTimer - 35
      end
    else
      tree.snowRegenTimer = 0
    end
  end
end

local function add_ground_trace(x, y, t)
  x, y = floor(x + 0.5), floor(y + 0.5)
  if x < 0 or x >= W or y < GY or y >= ROWS then return end
  local index = (y - GY) * W + x + 1
  groundTraces[index] = settings.groundTracePermanent and -1 or t + settings.groundTraceSeconds
end

local function update_ground_trace(input, t)
  if not input.inside then
    previousMouseX, previousMouseY = nil, nil
    return
  end
  if input.moved then
    local x0, y0 = previousMouseX or input.x, previousMouseY or input.y
    local dx, dy = input.x - x0, input.y - y0
    local steps = min(256, max(1, ceil(max(abs(dx), abs(dy)))))
    for i = 0, steps do
      local k = i / steps
      add_ground_trace(x0 + dx * k, y0 + dy * k, t)
    end
  end
  previousMouseX, previousMouseY = input.x, input.y
end

local function update_fire_gust(input, dt)
  fireGust.strength = fireGust.strength * exp(-dt * 0.9)
  local speed = sqrt(input.vx * input.vx + input.vy * input.vy)
  if not input.moved or speed <= 0.1 then return end

  local moveX, moveY = input.dx, input.dy
  local distance = sqrt(moveX * moveX + moveY * moveY)
  if distance < 0.01 then
    moveX, moveY, distance = input.vx, input.vy, speed
  end
  local startX, startY = input.x - moveX, input.y - moveY
  local samples = min(64, max(1, ceil(distance)))
  local bestDistance, hitX, hitY = math.huge, nil, nil
  local fireCenterX, fireCenterY = FX + FW / 2, FY + FH / 2
  for i = 0, samples do
    local k = i / samples
    local x, y = startX + moveX * k, startY + moveY * k
    if x >= FX - 2 and x < FX + FW + 2 and y >= FY - 2 and y < GY then
      local dx, dy = (x - fireCenterX) / FW, (y - fireCenterY) / FH
      local d = dx * dx + dy * dy
      if d < bestDistance then
        bestDistance, hitX, hitY = d, x, y
      end
    end
  end
  if hitX then
    fireGust.x, fireGust.y = hitX, hitY
    fireGust.dx, fireGust.dy = moveX / distance, moveY / distance
    fireGust.strength = max(fireGust.strength,
      min(3.2, speed / 9) * settings.mouseFirePower)
  end
end

local function update_mouse_flakes(input, dt, t)
  if input.pressed then handle_mouse_press(input) end

  local gather = input.inside and input.down
  local held = 0
  for _, f in ipairs(flakes) do
    if f.held then
      if gather then
        held = held + 1
      else
        f.held, f.holdX, f.holdY = false, nil, nil
      end
    end
  end
  if gather and held < settings.mouseSnowLimit then
    for _, f in ipairs(flakes) do
      if not f.held and held < settings.mouseSnowLimit then
        local dx, dy = f.x - input.x, f.y - input.y
        if dx * dx + dy * dy <= settings.mouseSnowRadius ^ 2 then
          f.held = true
          f.holdX, f.holdY = dx * 0.35, dy * 0.35
          held = held + 1
        end
      end
    end
  end

  for i = #flakes, 1, -1 do
    local f = flakes[i]
    if f.held then
      local pull = min(1, dt * 10 * settings.mouseSnowPull)
      f.x = f.x + (input.x + f.holdX - f.x) * pull
      f.y = f.y + (input.y + f.holdY - f.y) * pull
    else
      if f.kickTime > 0 then
        f.kickTime = max(0, f.kickTime - dt)
        f.kickX = f.kickX * exp(-2.4 * dt)
        f.kickY = f.kickY + 8 * dt
        if f.kickTime == 0 then f.kickX, f.kickY = 0, 0 end
      end
      f.x = (f.x + (settings.wind
        + sin(t * f.sw + f.ph) * 0.4 * f.l * settings.snowDrift
        + f.kickX) * dt) % W
      f.y = f.y + (f.v + f.kickY) * dt
    end

    if f.extra and not f.held then f.life = f.life - dt end
    if f.extra and (f.y >= GY or f.life <= 0) then
      flakes[i] = flakes[#flakes]
      flakes[#flakes] = nil
    else
      if not f.extra and not f.held and f.y >= GY then resetFlake(f, false) end
      put(f.x, f.y, LAYER_CH[f.l], LAYER_COL[f.l])
    end
  end
end

function update(t, dt, input)
  input = input or {
    inside = false, down = false, pressed = false, moved = false,
    x = 0, y = 0, vx = 0, vy = 0
  }
  clear(background)

  for _, s in ipairs(stars) do
    local b = s.twinkle and sin(t * s.speed * settings.starsSpeed + s.ph) or -1
    local bright = b > 0.72
    put(s.x, s.y, bright and "+" or ".", bright and s.bright or s.color)
  end

  update_ground_trace(input, t)
  update_fire_gust(input, dt)
  update_tree_snow(dt)
  for _, tree in ipairs(trees) do
    local top = GY - tree.h - 2
    drawTree(tree, top, t)
  end
  update_mouse_flakes(input, dt, t)

  glowTimer = glowTimer - dt
  if glowTimer <= 0 then
    glowTimer = 0.33
    glow = (0.85 + rand() * 0.3) * settings.glowPower
  end

  for y = GY, ROWS - 1 do
    local row = (y - GY) * W
    for x = 0, W - 1 do
      local i = row + x
      local l = min(6, floor(max(0, 1 - groundD[i]) * glow * 6))
      local bg = GROUND_BG[l]
      put(x, y, groundCh[i], GROUND_FG[l], bg)
      local traceExpiry = groundTraces[i + 1]
      if traceExpiry then
        if traceExpiry == -1 or traceExpiry > t then
          put(x, y, "*", 0x111827, bg)
        else
          groundTraces[i + 1] = nil
        end
      end
    end
  end

  -- Crossed support logs and a broad base, with the ground kept on row GY.
  local logY = GY - 1
  local cx = FX + floor(FW / 2)
  local rise = max(2, floor(FH * 0.22))
  local span = max(3, floor(FW * 0.42))
  for i = 0, span do
    local y = logY - floor(i * rise / span)
    local leftX, rightX = cx - span + i, cx + span - i
    local bark = i % 4 == 0 and "#" or "="
    put(leftX, y, i % 4 == 0 and "/" or bark, 0x70401f)
    put(leftX, y - 1, bark, 0x5c3317)
    put(rightX, y, i % 4 == 0 and "\\" or bark, 0x70401f)
    put(rightX, y - 1, bark, 0x5c3317)
  end
  local baseLeft, baseRight = cx - span - 1, cx + span + 1
  for x = baseLeft, baseRight do
    local i = x - baseLeft
    put(x, logY, i % 5 == 0 and "#" or "=", i % 5 == 0 and 0x80502d or 0x5c3317)
    if i % 6 == 2 then put(x, logY - 1, ".", 0x9a5428) end
  end

  fireClock = fireClock + dt * settings.fireSpeed
  while fireClock >= 1 / 30 do
    stepFire()
    fireClock = fireClock - 1 / 30
  end
  for y = 0, FH - 1 do
    local windShift = floor(settings.wind * 0.035 * (FH - 1 - y) + 0.5)
    local vertical = y / max(1, FH - 1)
    local halfWidth = FW * 0.5 * (0.15 + 0.85 * vertical ^ 0.78)
    local center = (FW - 1) * 0.5
    for x = 0, FW - 1 do
      local screenX, screenY = FX + x, FY + y
      local dx, dy = (screenX - fireGust.x) / 5, (screenY - fireGust.y) / 3
      local influence = exp(-(dx * dx + dy * dy)) * fireGust.strength
      local warp = 4 * influence
      local sourceX = clamp(x - floor(fireGust.dx * warp + 0.5), 0, FW - 1)
      local sourceY = clamp(y - floor(fireGust.dy * warp + 0.5), 0, FH - 1)
      local h = clamp(floor(heat[sourceY * FW + sourceX] or 0) - floor(influence * 10), 0, MAXH)
      if h > 3 and abs(x - center) <= halfWidth then
        local ch = fireCh[h] or " "
        put(FX + x + windShift, FY + y, ch, fireCol[h] or 0xff8420)
      end
    end
  end

  if rand() < dt * 5 then
    sparks[#sparks + 1] = {
      x = FX + FW / 2 + (rand() - 0.5) * FW * 0.4,
      y = FY + FH * 0.50,
      vy = -(4 + rand() * 6),
      life = 1 + rand() * 1.5,
      ph = rand() * 6.28
    }
  end
  for i = #sparks, 1, -1 do
    local s = sparks[i]
    s.life = s.life - dt
    if s.life <= 0 then
      sparks[i] = sparks[#sparks]; sparks[#sparks] = nil
    else
      s.x = s.x + (sin(t * 4 + s.ph) * 2 + settings.wind * 0.35) * dt
      s.y = s.y + s.vy * dt
      put(s.x, s.y, s.life > 0.8 and "'" or ".", s.life > 0.8 and 0xffc04d or 0xa8481a)
    end
  end

  coalTimer = coalTimer - dt
  if coalTimer <= 0 then
    coalTimer = 1.2 + rand() * 3.8
    if rand() < clamp(settings.coalRate, 0, 1) then
      local side = rand() < 0.5 and -1 or 1
      coals[#coals + 1] = {
        x = side < 0 and FX + FW * 0.18 or FX + FW * 0.82,
        y = FY + FH * (0.55 + rand() * 0.25),
        vx = side * (1.2 + rand() * 2.2) + settings.wind * 0.25,
        vy = -(0.3 + rand() * 1.0),
        life = 1.1 + rand() * 1.2
      }
    end
  end
  for i = #coals, 1, -1 do
    local c = coals[i]
    c.life = c.life - dt
    c.vy = c.vy + dt * 3
    c.x = c.x + c.vx * dt
    c.y = c.y + c.vy * dt
    if c.life <= 0 or c.y >= GY - 1 or c.x < 0 or c.x >= W then
      coals[i] = coals[#coals]; coals[#coals] = nil
    else
      put(c.x, c.y, c.life > 0.8 and "*" or ".", c.life > 0.8 and 0xff7335 or 0x5a1d12)
    end
  end
end
