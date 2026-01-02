# Quantum Tides - Tides V2 定制固件

**Quantum Tides** 是为 Mutable Instruments Tides V2 (2018) 硬件开发的一个替代固件。

它保留了 Tides V2 原有的所有功能（波形发生器/LFO），并增加了一个全新的 **"Quantum Mode"（量子模式）**。在这个模式下，Tides 变身为一个**4通道随机电压生成器与量化器**，其设计灵感来源于 Mutable Instruments Marbles（随机）和 Plaits（和声）。

这个固件旨在将 Tides 变成你 Eurorack 系统中的"生成性大脑"，提供量化的旋律、和声伴奏以及随机节奏。

## 📥 安装与升级

1. **下载**: 获取编译好的 `tides2.wav` 文件。
2. **连接**: 将音频播放设备（电脑/手机）的输出连接到 Tides 的 **FM 输入**接口。
3. **进入升级模式**:
   - 确保 Tides 已断电。
   - 按住模块的 **OUTPUT** 按钮不放。
   - 给模块上电。
   - 此时所有 LED 会亮起，松开按钮。
4. **播放**: 播放 wav 文件。
   - 升级过程中 LED 会像进度条一样显示。
   - 如果所有 LED 闪烁红灯，说明信号电平过低，请以此增加音量并重试。
   - 如果遇到 Signal Error，请检查线材或关闭音效增强功能。
5. **完成**: 升级完成后，模块会自动重启进入正常模式。

---

## 🎛 模式切换

Quantum Tides 在启动时默认进入 **原厂标准模式**。

- **进入/退出 Quantum 模式**: 长按（约 1 秒）**OUTPUT 按钮**（最上方的按钮）。
- **视觉指示**:
  - **原厂模式**: OUTPUT LED 显示绿色、黄色或熄灭（取决于频率范围）。
  - **Quantum 模式**: OUTPUT LED 会 **快速闪烁红色/红绿色**，以示区别。

---

## 🌌 Quantum 模式功能详解

在 Quantum 模式下，Tides 不再生成平滑的循环波形，而是基于时钟生成 **随机游走电压 (Stochastic Random Voltages)**，并通过内置量化器输出。

### 核心概念

- **时钟源**:
  - 如果 **TRIG 输入** 未接线：模块使用内部时钟，速度由 Frequency 旋钮控制。
  - 如果 **TRIG 输入** 接线：模块由外部时钟驱动，Frequency 旋钮控制时钟分频/倍频比率。
- **信号流**:
  时钟触发 -> 生成随机数 -> 经过 Shape 分布处理 -> 经过 Smoothness 滑音处理 -> 经过 Slope 量化 -> 经过 Shift 和声处理 -> 输出。

### 🎚 旋钮功能映射

| 物理旋钮 | 功能名称 | 详细说明 |
| :--- | :--- | :--- |
| **FREQUENCY** | **Rate / Div** | **速率/分频**: <br>- **无 Trig**: 控制内部随机生成的速率。<br>- **有 Trig**: 控制外部时钟的分频/倍频比率。 |
| **SHAPE** | **Distribution** | **分布偏好 (Bias)**: 控制随机电压的分布概率。<br>- **12点**: 均匀分布。<br>- **顺时针**: 偏向高音/高电压。<br>- **逆时针**: 偏向低音/低电压。 |
| **SLOPE** | **Scale** | **音阶选择**: 选择量化器的音阶（见下方音阶表）。<br>旋钮从左到右依次切换 6 种不同的调式。 |
| **SMOOTHNESS** | **Slew** | **滑音/平滑度**: <br>- **逆时针 (CCW)**: 阶梯状电压 (Stepped)，音高瞬间变化。<br>- **顺时针 (CW)**: 引入滑音 (Slew/Glide)，使电压在音符间平滑过渡。 |
| **SHIFT/LEVEL** | **Spread** | **和声/音程散布**: <br>- 控制 Output 2 和 Output 4 相对于主旋律 (Output 1) 的音程偏移量。<br>- 产生和弦或对位旋律效果。 |

### 🎹 量化音阶 (通过 SLOPE 旋钮选择)

固件内置了 6 种常用音阶，通过旋转 **SLOPE** 旋钮进行选择：

1. **Chromatic** (半音阶 / 十二平均律)
2. **Major** (自然大调)
3. **Minor** (自然小调)
4. **Pentatonic Major** (大调五声)
5. **Pentatonic Minor** (小调五声)
6. **Octaves/Fifths** (八度和五度)

---

## 🔌 输出端口分配 (Quantum 模式)

Tides V2 的四个输出口被重新分配，以最大化系统的联动性：

- **OUTPUT 1 (Main CV)**:
  - **主旋律输出**。
  - 这里的电压是主要生成的随机游走结果，经过了量化。
  - *建议连接*: 主振荡器 (如 Plaits) 的 V/Oct 输入。

- **OUTPUT 2 (Harmony CV 1)**:
  - **和声输出**。
  - 基于主旋律，并叠加了由 **SHIFT** 旋钮控制的偏移量。
  - *建议连接*: 副振荡器、滤波器的 Cutoff、或 Plaits 的 Harmonics/Timbre 参数。

- **OUTPUT 3 (Gate/Trigger)**:
  - **随机触发门信号**。
  - 每当生成一个新的随机音符时，这里会输出一个高电平脉冲 (Gate/Trig)。
  - *注意*: 此输出不随 Shift 变化，它专门用于触发包络。
  - *建议连接*: 包络发生器 (如 Miasma) 的 Trig 输入。

- **OUTPUT 4 (Harmony CV 2)**:
  - **第二和声输出**。
  - 类似于 Output 2，但具有不同的音程偏移计算方式，提供更丰富的复音纹理。
  - *建议连接*: 效果器参数 (如 Mimeophon 的 Rate/Zone) 或立体声滤波器的另一侧。

---

## 🛠 典型用法示例

### 1. 自动生成旋律 (Self-Running Patch)

- **配置**: 拔掉 TRIG 输入。

- **连线**: Tides Out 1 -> Plaits V/Oct。
- **操作**:
  - 调节 **Frequency** 改变旋律速度。
  - 调节 **Slope** 选择一个悦耳的音阶 (如五声调式)。
  - 调节 **Shape** 让旋律在低音区徘徊或向高音区爬升。

### 2. 节奏锁定的随机 (Clocked Random)

- **配置**: 将外部音序器或 LFO (如 øchd) 的方波接入 **TRIG**。

- **连线**:
  - Tides Out 1 -> 振荡器 V/Oct。
  - Tides Out 3 -> 包络发生器 (Miasma) Trig。
  - 包络输出 -> VCA CV。
- **效果**: Tides 会跟随你的系统速度生成旋律。Tides 的 Out 3 确保只有在音高变化时才触发声音，创造出紧致的节奏感。

### 3. 多复音/和弦生成 (Poly/Chord Generation)

- **配置**: 使用一个支持多路 V/Oct 的振荡器 (或 Plaits 的 Chord 模式)。

- **连线**:
  - Tides Out 1 -> 根音 V/Oct。
  - Tides Out 2 -> 和弦参数 (Harmonics) 或第二振荡器 V/Oct。
- **操作**: 旋转 **Shift** 旋钮。你会听到伴随随机旋律的自动和声变化，从单音铺底 (Unison) 到宽广的和弦 (Wide Spread)。

---

## ⚠️ 注意事项

- **电压范围**: 输出电压通常为双极性 (-5V 到 +5V)，这是为了适应最广泛的 LFO 和 VCO 调制需求。如果你的振荡器只需要正电压 (0-8V)，可能需要外部衰减器或偏置 (Offset)。
- **校准**: 此固件使用 Tides 原厂的校准数据。如果音高不准，请先在原厂模式下按照官方流程进行校准。

---

## 编译

```bash
docker pull archont94/mutable-env:latest

docker run --rm --platform linux/amd64 \
  -e PYTHONPATH=. \
  -v "/Users/yyf/Documents/GitHub/eurowings":/eurorack \
  -w /eurorack \
  archont94/mutable-env:latest \
  make -f tides2/makefile wav
```

*Custom firmware tailored for Eurorack modular systems.*
*Based on Mutable Instruments Tides source code (MIT License).*
