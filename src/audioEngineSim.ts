import {
  AudioConfig,
  AudioDeviceInfo,
  AudioMetrics,
  AudioState,
  DriverType,
  ChannelStripState,
  MasterStripState,
} from './types';

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
  private wasRunningBeforeDisconnect: boolean = false;
  private autoReconnectTimer: number | null = null;

  // 4-Channel Mixer State
  private ch1: ChannelStripState = {
    id: 1,
    name: 'CH1 MIC',
    source: 'Hardware In 1',
    gainDb: 0,
    panOrBalance: 0,
    faderDb: 0,
    muted: false,
    solo: false,
    peakL: 0,
    peakR: 0,
    clipped: false,
  };

  private ch2: ChannelStripState = {
    id: 2,
    name: 'CH2 INST',
    source: 'Hardware In 2',
    gainDb: 0,
    panOrBalance: 0,
    faderDb: 0,
    muted: false,
    solo: false,
    peakL: 0,
    peakR: 0,
    clipped: false,
  };

  private ch34: ChannelStripState = {
    id: 3,
    name: 'CH3/4 MEDIA',
    source: 'Hardware In 1-2',
    gainDb: 0,
    panOrBalance: 0,
    faderDb: 0,
    muted: false,
    solo: false,
    peakL: 0,
    peakR: 0,
    clipped: false,
  };

  private master: MasterStripState = {
    name: 'MASTER',
    source: 'Out 1/2 (Main)',
    faderDb: 0,
    muted: false,
    peakL: 0,
    peakR: 0,
    clipped: false,
  };

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

      // Realtime Audio Callback with 4-Channel Mixing
      this.scriptNode.onaudioprocess = (audioProcessingEvent) => {
        const t0 = performance.now();
        const inputBuffer = audioProcessingEvent.inputBuffer;
        const outputBuffer = audioProcessingEvent.outputBuffer;

        const in0 = inputBuffer.getChannelData(0);
        const in1 = inputBuffer.numberOfChannels > 1 ? inputBuffer.getChannelData(1) : in0;

        const outL = outputBuffer.getChannelData(0);
        const outR = outputBuffer.getChannelData(1);

        const len = inputBuffer.length;

        // Solo Logic
        const ch1Solo = this.ch1.solo;
        const ch2Solo = this.ch2.solo;
        const ch34Solo = this.ch34.solo;
        const hasSolo = ch1Solo || ch2Solo || ch34Solo;

        const ch1Audible = !this.ch1.muted && (!hasSolo || ch1Solo) && this.ch1.faderDb > -60;
        const ch2Audible = !this.ch2.muted && (!hasSolo || ch2Solo) && this.ch2.faderDb > -60;
        const ch34Audible = !this.ch34.muted && (!hasSolo || ch34Solo) && this.ch34.faderDb > -60;

        const ch1GainLin = Math.pow(10, (this.ch1.gainDb + this.ch1.faderDb) / 20);
        const ch2GainLin = Math.pow(10, (this.ch2.gainDb + this.ch2.faderDb) / 20);
        const ch34GainLin = Math.pow(10, (this.ch34.gainDb + this.ch34.faderDb) / 20);

        const ch1Angle = (this.ch1.panOrBalance + 1.0) * (Math.PI / 4.0);
        const ch1PanL = Math.cos(ch1Angle);
        const ch1PanR = Math.sin(ch1Angle);

        const ch2Angle = (this.ch2.panOrBalance + 1.0) * (Math.PI / 4.0);
        const ch2PanL = Math.cos(ch2Angle);
        const ch2PanR = Math.sin(ch2Angle);

        const ch34Bal = this.ch34.panOrBalance;
        const ch34PanL = ch34Bal <= 0 ? 1.0 : 1.0 - ch34Bal;
        const ch34PanR = ch34Bal >= 0 ? 1.0 : 1.0 + ch34Bal;

        const masterAudible = !this.master.muted && this.master.faderDb > -60;
        const masterGainLin = masterAudible ? Math.pow(10, this.master.faderDb / 20) : 0;

        let pk1 = 0, pk2 = 0, pk34L = 0, pk34R = 0, pkML = 0, pkMR = 0;

        for (let i = 0; i < len; i++) {
          const s0 = in0[i];
          const s1 = in1[i];

          const abs0 = Math.abs(s0);
          const abs1 = Math.abs(s1);
          if (abs0 > pk1) pk1 = abs0;
          if (abs1 > pk2) pk2 = abs1;
          if (abs0 > pk34L) pk34L = abs0;
          if (abs1 > pk34R) pk34R = abs1;

          let busL = 0;
          let busR = 0;

          if (ch1Audible) {
            const sig = s0 * ch1GainLin;
            busL += sig * ch1PanL;
            busR += sig * ch1PanR;
          }
          if (ch2Audible) {
            const sig = s1 * ch2GainLin;
            busL += sig * ch2PanL;
            busR += sig * ch2PanR;
          }
          if (ch34Audible) {
            busL += s0 * ch34GainLin * ch34PanL;
            busR += s1 * ch34GainLin * ch34PanR;
          }

          const outSampleL = busL * masterGainLin;
          const outSampleR = busR * masterGainLin;

          outL[i] = outSampleL;
          outR[i] = outSampleR;

          const absOutL = Math.abs(outSampleL);
          const absOutR = Math.abs(outSampleR);
          if (absOutL > pkML) pkML = absOutL;
          if (absOutR > pkMR) pkMR = absOutR;
        }

        this.ch1.peakL = pk1;
        this.ch1.peakR = pk1;
        this.ch1.clipped = pk1 >= 1.0;

        this.ch2.peakL = pk2;
        this.ch2.peakR = pk2;
        this.ch2.clipped = pk2 >= 1.0;

        this.ch34.peakL = pk34L;
        this.ch34.peakR = pk34R;
        this.ch34.clipped = pk34L >= 1.0 || pk34R >= 1.0;

        this.master.peakL = pkML;
        this.master.peakR = pkMR;
        this.master.clipped = pkML >= 1.0 || pkMR >= 1.0;

        this.inputLevels = [pk1, pk2];
        this.outputLevels = [pkML, pkMR];

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
    const wasRunning = (this.state === 'Running');
    if (wasRunning) {
      this.wasRunningBeforeDisconnect = true;
      this.state = 'Recovering';
      this.stop();
      this.state = 'Error';
      this.disconnectListeners.forEach((cb) => cb(`Device '${this.config.deviceName}' disconnected. Waiting for device reconnection...`));
    } else {
      this.state = 'Error';
      this.disconnectListeners.forEach((cb) => cb(`Device '${this.config.deviceName}' is offline.`));
    }

    // Schedule automatic simulated reconnect after 2 seconds if device was running
    if (this.autoReconnectTimer) {
      clearTimeout(this.autoReconnectTimer);
      this.autoReconnectTimer = null;
    }

    if (this.wasRunningBeforeDisconnect) {
      this.autoReconnectTimer = window.setTimeout(() => {
        if (this.state === 'Error' || this.state === 'Recovering') {
          this.state = 'Recovering';
          this.disconnectListeners.forEach((cb) => cb(`Device '${this.config.deviceName}' detected. Reconnecting...`));
          setTimeout(() => {
            this.state = 'Ready';
            if (this.wasRunningBeforeDisconnect) {
              this.start().then(() => {
                this.wasRunningBeforeDisconnect = false;
                this.disconnectListeners.forEach((cb) => cb(`Device '${this.config.deviceName}' reconnected successfully.`));
              });
            }
          }, 600);
        }
      }, 2500);
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

  // Mixer Getters & Setters
  public getChannel1(): ChannelStripState {
    return { ...this.ch1 };
  }

  public getChannel2(): ChannelStripState {
    return { ...this.ch2 };
  }

  public getChannel34(): ChannelStripState {
    return { ...this.ch34 };
  }

  public getMaster(): MasterStripState {
    return { ...this.master };
  }

  public setChannelParam(channelId: 1 | 2 | 3, param: 'gainDb' | 'panOrBalance' | 'faderDb' | 'muted' | 'solo', value: number | boolean) {
    const ch = channelId === 1 ? this.ch1 : channelId === 2 ? this.ch2 : this.ch34;
    if (param === 'gainDb') ch.gainDb = value as number;
    else if (param === 'panOrBalance') ch.panOrBalance = value as number;
    else if (param === 'faderDb') ch.faderDb = value as number;
    else if (param === 'muted') ch.muted = value as boolean;
    else if (param === 'solo') ch.solo = value as boolean;
  }

  public setMasterParam(param: 'faderDb' | 'muted', value: number | boolean) {
    if (param === 'faderDb') this.master.faderDb = value as number;
    else if (param === 'muted') this.master.muted = value as boolean;
  }
}

export const globalAudioEngine = new AudioEngineSim();
