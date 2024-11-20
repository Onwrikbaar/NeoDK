import NeoDK from './neodk.js';
import { createApp, ref, toRaw, computed } from 'vue';

class NeoDKBoxPowerVM extends NeoDK.BoxPower {
    constructor() {
        super();

    }

    #batteryVoltage = ref(super._batteryVoltage);
    #capacitorVoltage = ref(super._capacitorVoltage);
    #primaryCurrent = ref(super._primaryCurrent);

    get BatteryVoltage() {
        return toRaw(this).#batteryVoltage.value;
    }
    set BatteryVoltage(value){
        toRaw(this).#batteryVoltage.value = value;
    }
    
    get CapacitorVoltage() {
        return toRaw(this).#capacitorVoltage.value;
    }
    set CapacitorVoltage(value){
        toRaw(this).#capacitorVoltage.value = value;
    }
    
    get PrimaryCurrent() {
        return toRaw(this).#primaryCurrent.value;
    }
    set PrimaryCurrent(value){
        toRaw(this).#primaryCurrent.value = value;
    }
}

class NeoDKStateVM extends NeoDK.State {
    constructor() {
        super();
        this.power = new NeoDKBoxPowerVM();
    }

    #playState = ref(super._playState);
    #intensity = ref(super._intensity);
    #currentPattern = ref(super._currentPattern);
    #availablePatterns = ref(super._availablePatterns);

    get PlayState() {
        return toRaw(this).#playState.value;
    }
    set PlayState(value) {
        toRaw(this).#playState.value = value;
    }

    get Intensity() {
        return toRaw(this).#intensity.value;
    }
    set Intensity(value) {
        toRaw(this).#intensity.value = value;
    }

    get CurrentPattern() {
        return toRaw(this).#currentPattern.value;
    }
    set CurrentPattern(value) {
        toRaw(this).#currentPattern.value = value;
    }

    get AvailablePatterns() {
        return toRaw(this).#availablePatterns.value;
    }
    set AvailablePatterns(value) {
        toRaw(this).#availablePatterns.value = value;
    }
}

class NeoDKVM extends NeoDK {
    setIntensity = (intensity) => super.setIntensity(intensity);

    selectPattern = (pattern) => super.selectPattern(pattern);

    changeIntensity = (amount) => { super.setIntensity(this.state.Intensity + amount); }

    resume = () => super.setPlayState(NeoDK.ChangeStateCommand.play);
    pause = () => super.setPlayState(NeoDK.ChangeStateCommand.pause);
    stop = () => super.setPlayState(NeoDK.ChangeStateCommand.stop);

    paused = computed(() => ['paused', 'stopped'].includes(this.state.PlayState));
}

const app = createApp({
    data() {
        return {
            isBrowserSupported: NeoDK.browserSupported,
            devices: []
        };
    },
    methods: {
        async connect() {
            try {
                var device = new NeoDKVM({ logger: { log: console.log, deubug: console.log }, state: new NeoDKStateVM() });
                var selected = await device.selectPort();
                if (selected) {
                    this.devices.push(device);
                }
            } catch (error) {
                alert('Connect failed:' + error);
            }
        },
        async refreshState() {
            try {
                this.devices.forEach(element => {
                    toRaw(element).refreshVoltages();
                });
                setTimeout(this.refreshState, 1000 * 60 * 5);
            } catch (error) {
                console.error('Failed to fetch state', error);
            }
        }
    },
    mounted() {
        if (this.isBrowserSupported()) {
            this.refreshState();
        }
    }
});

app.mount('#app');

