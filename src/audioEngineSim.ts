import { AudioConfig, AudioDeviceInfo, AudioMetrics, AudioState, DriverType } from './types';

export const DEFAULT_DEVICES: AudioDeviceInfo[] = [
  {
    id: 'asio_focusrite',
    name: 'Focusrite USB ASIO',
    driverType: 'ASIO',
    maxInputChannels: 2,
    maxOutputChannels: 2,
    supportedSampleRates: [44100, 48000, 88200, 96000, 192000],
    supportedBufferSizes: [32, 64, 128, 256, 512, 1024],
    isDefault: true,
  },
  {
    id: 'asio_rme',
    name: 'RME Fireface ASIO',
    driverType: 'ASIO',
    maxInputChannels: 8,
    maxOutputChannels: 8,
    supportedSampleRates: [44100, 48000, 88200, 96000, 192000],
    supportedBufferSizes: [32, 64, 128, 256, 512, 1024],
  },
  {
    id: 'asio_generic',
    name: 'Generic Low Latency ASIO Driver',
    driverType: 'ASIO',
    maxInputChannels: 2,
    maxOutputChannels: 2,
    supportedSampleRates: [44100, 48000, 96000],
    supportedBufferSizes: [64, 128, 256, 512],
  },
  {
    id: 'wasapi_realtek',
    name: 'Realtek High Definition Audio (WASAPI)',
    driverType: 'WASAPI',
    maxInputChannels: 2,
    maxOutputChannels: 2,
    supportedSampleRates: [44100, 48000, 96000],
    supportedBufferSizes: [64, 128, 256, 512, 1024],
    isDefault: true,
  },
  {
    id: 'wasapi_usb',
    name: 'USB Audio Codec (WASAPI)',
    driverType: 'WASAPI',
    maxInputChannels: 2,
    maxOutputChannels: 2,
    supportedSampleRates: [44100, 48000],
    supportedBufferSizes: [128, 256, 512, 1024],
  },
  {
    id: 'wasapi_headset',
    name: 'Headphones / Communications (WASAPI)',
    driverType: 'WASAPI',
    maxInputChannels: 1,
    maxOutputChannels: 2,
    supportedSampleRates: [48000],
    supportedBufferSizes: [128, 256, 512],
  },
];

export class AudioEngineSim {
  private config: AudioConfig;
  private state: AudioState = 'Ready';
  private xrunCount: number = 0;
  private audioCtx: AudioContext | null = null;
  private sourceNode: AudioNode | null = null;
  private scriptNode: ScriptProcessorNode | null = null;
  private passthroughGain: GainNode | null = null;
  private micStream: MediaStream | null = null;
  private lastProcessingTimeMs: number = 0.08;
  private disconnectListeners: Array<(msg: string) => void> = [];
  private inputLevels: [number, number] = [0, 0];
  private outputLevels: [number, number] = [0, 0];
  private testSignalType: 'mic' | 'oscillator' | 'silence' = 'oscillator';

  constructor() {
    this.config = {
      driverType: 'ASIO',
      deviceName: 'Focusrite USB ASIO',
      sampleRate: 48000,
      bufferSize: 128,
      inputChannelCount: 2,
      outputChannelCount: 2,
    };
  }

  public getConfig(): AudioConfig {
    return { ...this.config };
  }

  public getState(): AudioState {
    return this.state;
  }

  public getAvailableDevices(driver: DriverType): AudioDeviceInfo[] {
    return DEFAULT_DEVICES.filter((d) => d.driverType === driver);
  }

  public setDriver(driver: DriverType): boolean {
    const wasRunning = this.state === 'Running';
    if (wasRunning) {
      this.stop();
    }
    this.config.driverType = driver;
    const devices = this.getAvailableDevices(driver);
    if (devices.length > 0) {
      this.config.deviceName = devices[0].name;
      this.config.inputChannelCount = devices[0].maxInputChannels >= 2 ? 2 : devices[0].maxInputChannels;
      this.config.outputChannelCount = devices[0].maxOutputChannels >= 2 ? 2 : devices[0].maxOutputChannels;
      if (!devices[0].supportedSampleRates.includes(this.config.sampleRate)) {
        this.config.sampleRate = devices[0].supportedSampleRates[0];
      }
      if (!devices[0].supportedBufferSizes.includes(this.config.bufferSize)) {
        this.config.bufferSize = devices[0].supportedBufferSizes[0];
      }
    }
    if (wasRunning) {
      this.start();
    }
    return true;
  }

  public setDevice(deviceName: string): boolean {
    const wasRunning = this.state === 'Running';
    if (wasRunning) {
      this.stop();
    }
    const dev = DEFAULT_DEVICES.find((d) => d.name === deviceName);
    if (dev) {
      this.config.deviceName = dev.name;
      this.config.inputChannelCount = dev.maxInputChannels >= 2 ? 2 : dev.maxInputChannels;
      this.config.outputChannelCount = dev.maxOutputChannels >= 2 ? 2 : dev.maxOutputChannels;
      if (!dev.supportedSampleRates.includes(this.config.sampleRate)) {
        this.config.sampleRate = dev.supportedSampleRates[0];
      }
      if (!dev.supportedBufferSizes.includes(this.config.bufferSize)) {
        this.config.bufferSize = dev.supportedBufferSizes[0];
      }
    }
    if (wasRunning) {
      this.start();
    }
    return true;
  }

  public setSampleRate(rate: number): boolean {
    const wasRunning = this.state === 'Running';
    if (wasRunning) {
      this.stop();
    }
    this.config.sampleRate = rate;
    if (wasRunning) {
      this.start();
    }
    return true;
  }

  public setBufferSize(size: number): boolean {
    const wasRunning = this.state === 'Running';
    if (wasRunning) {
      this.stop();
    }
    this.config.bufferSize = size;
    if (wasRunning) {
      this.start();
    }
    return true;
  }

  public setTestSignalType(type: 'mic' | 'oscillator' | 'silence') {
    this.testSignalType = type;
    if (this.state === 'Running') {
      this.stop();
      this.start();
    }
  }

  public getTestSignalType() {
    return this.testSignalType;
  }

  public calculateLatencies() {
    const bufferDurationMs = (this.config.bufferSize / this.config.sampleRate) * 1000;
    if (this.config.driverType === 'ASIO') {
      const converterLatencyMs = (32 / this.config.sampleRate) * 1000;
      return {
        inMs: bufferDurationMs + converterLatencyMs,
        outMs: bufferDurationMs + converterLatencyMs,
      };
    } else {
      return {
        inMs: bufferDurationMs + 8.5,
        outMs: bufferDurationMs + 8.5,
      };
    }
  }

  public async start(): Promise<boolean> {
    if (this.state === 'Running') return true;
    this.state = 'Initializing';

    try {
      // Setup Web Audio graph for true realtime audio callback & passthrough
      const AudioCtxClass = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      this.audioCtx = new AudioCtxClass();
      if (this.audioCtx.state === 'suspended') {
        await this.audioCtx.resume();
      }

      const bufferSize = 256; // Web Audio buffer chunk
      this.scriptNode = this.audioCtx.createScriptProcessor(bufferSize, 2, 2);

      // Realtime Audio Callback (Passthrough: In 1 -> Out 1, In 2 -> Out 2)
      this.scriptNode.onaudioprocess = (audioProcessingEvent) => {
        const t0 = performance.now();
        const inputBuffer = audioProcessingEvent.inputBuffer;
        const outputBuffer = audioProcessingEvent.outputBuffer;

        const inL = inputBuffer.getChannelData(0);
        const inR = inputBuffer.numberOfChannels > 1 ? inputBuffer.getChannelData(1) : inL;

        const outL = outputBuffer.getChannelData(0);
        const outR = outputBuffer.getChannelData(1);

        let maxL = 0;
        let maxR = 0;

        // Hard realtime loop - direct copy without allocation
        for (let sample = 0; sample < inputBuffer.length; sample++) {
          outL[sample] = inL[sample];
          outR[sample] = inR[sample];

          const absL = Math.abs(inL[sample]);
          const absR = Math.abs(inR[sample]);
          if (absL > maxL) maxL = absL;
          if (absR > maxR) maxR = absR;
        }

        this.inputLevels = [maxL, maxR];
        this.outputLevels = [maxL, maxR];

        const t1 = performance.now();
        this.lastProcessingTimeMs = Math.max(0.04, t1 - t0);
      };

      if (this.testSignalType === 'mic') {
        try {
          this.micStream = await navigator.mediaDevices.getUserMedia({ audio: true });
          this.sourceNode = this.audioCtx.createMediaStreamSource(this.micStream);
          this.sourceNode.connect(this.scriptNode);
        } catch {
          // Fallback to stereo tone if mic blocked
          this.setupToneGenerator();
        }
      } else if (this.testSignalType === 'oscillator') {
        this.setupToneGenerator();
      }

      this.passthroughGain = this.audioCtx.createGain();
      this.passthroughGain.gain.value = 0.8; // Safe listening level

      this.scriptNode.connect(this.passthroughGain);
      this.passthroughGain.connect(this.audioCtx.destination);

      this.state = 'Running';
      return true;
    } catch {
      this.state = 'Error';
      return false;
    }
  }

  private setupToneGenerator() {
    if (!this.audioCtx || !this.scriptNode) return;
    const osc = this.audioCtx.createOscillator();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(440, this.audioCtx.currentTime); // 440 Hz standard A test tone
    const oscGain = this.audioCtx.createGain();
    oscGain.gain.value = 0.25;
    osc.connect(oscGain);
    oscGain.connect(this.scriptNode);
    osc.start();
    this.sourceNode = osc;
  }

  public stop() {
    if (this.state === 'Running') {
      this.state = 'Stopping';
      if (this.micStream) {
        this.micStream.getTracks().forEach((t) => t.stop());
        this.micStream = null;
      }
      if (this.audioCtx) {
        this.audioCtx.close().catch(() => {});
        this.audioCtx = null;
      }
      this.inputLevels = [0, 0];
      this.outputLevels = [0, 0];
      this.state = 'Ready';
    }
  }

  public triggerSimulatedXRun() {
    this.xrunCount++;
  }

  public simulateDeviceDisconnect() {
    if (this.state === 'Running') {
      this.state = 'Recovering';
      this.stop();
      this.state = 'Error';
      this.disconnectListeners.forEach((cb) => cb(`Device '${this.config.deviceName}' disconnected or bus timeout.`));
    } else {
      this.state = 'Error';
      this.disconnectListeners.forEach((cb) => cb(`Device '${this.config.deviceName}' is offline.`));
    }
  }

  public onDisconnect(listener: (msg: string) => void) {
    this.disconnectListeners.push(listener);
  }

  public getMetrics(): AudioMetrics {
    const latencies = this.calculateLatencies();
    const bufferDurationMs = (this.config.bufferSize / this.config.sampleRate) * 1000;

    return {
      inputLatencyMs: latencies.inMs,
      outputLatencyMs: latencies.outMs,
      sampleRate: this.config.sampleRate,
      bufferSize: this.config.bufferSize,
      processingTimeMs: this.state === 'Running' ? this.lastProcessingTimeMs : 0.0,
      bufferDurationMs,
      xrunCount: this.xrunCount,
      audioState: this.state,
    };
  }

  public getLevels() {
    return {
      inLeft: this.inputLevels[0],
      inRight: this.inputLevels[1],
      outLeft: this.outputLevels[0],
      outRight: this.outputLevels[1],
    };
  }
}

export const globalAudioEngine = new AudioEngineSim();
