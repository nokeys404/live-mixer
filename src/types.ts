export type DriverType = 'ASIO' | 'WASAPI';

export type AudioState =
  | 'Offline'
  | 'Initializing'
  | 'Ready'
  | 'Running'
  | 'Stopping'
  | 'Error'
  | 'Recovering';

export interface AudioDeviceInfo {
  id: string;
  name: string;
  driverType: DriverType;
  maxInputChannels: number;
  maxOutputChannels: number;
  supportedSampleRates: number[];
  supportedBufferSizes: number[];
  isDefault?: boolean;
}

export interface AudioConfig {
  driverType: DriverType;
  deviceName: string;
  sampleRate: number;
  bufferSize: number;
  inputChannelCount: number;
  outputChannelCount: number;
}

export interface AudioMetrics {
  inputLatencyMs: number;
  outputLatencyMs: number;
  sampleRate: number;
  bufferSize: number;
  processingTimeMs: number;
  bufferDurationMs: number;
  xrunCount: number;
  audioState: AudioState;
}

export interface ChannelStripState {
  id: number;
  name: string;
  source: string;
  gainDb: number;
  panOrBalance: number;
  faderDb: number;
  muted: boolean;
  solo: boolean;
  peakL: number;
  peakR: number;
  clipped: boolean;
}

export interface MasterStripState {
  name: string;
  source: string;
  faderDb: number;
  muted: boolean;
  peakL: number;
  peakR: number;
  clipped: boolean;
}

export interface TestResult {
  name: string;
  passed: boolean;
  message: string;
  durationMs: number;
}
