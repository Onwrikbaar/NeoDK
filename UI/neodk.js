"use strict";
/**
 * Represents a instance of NeoDK box that is connected to a serial port.
 * @class
 * @copyright 2024 Neostim B.V. All rights reserved.
 */
class NeoDK {

    /**
     * initialize instance
     * @constructor
     * @param { Object } dependencies - the dependencies for the NeoDK instance
     * @param { Object } dependencies.logger - The logger to use; needs to have methods: log, debug.
     * @param { NeoDK.State } dependencies.state - The custom state class, if you want to override its setters to be able to react on state changes
     *  @default console
     */
    constructor({ logger = console, state = new NeoDK.State() }) {
        this.#rx_frame = new Uint8Array(NeoDK.#StructureSize.FrameHeader + NeoDK.#StructureSize.MaxPayload);

        this.logger = logger;
        this.state = state;
        this._name = '';
    }

    /**
     * Check if browser supports WebSerial API
     * @returns true if browser supports WebSerial API, false otherwise
     */
    static browserSupported() { return 'serial' in navigator; }

    /**
     * Enum for allowed play states that can be set
     * @public
     * @enum
     * @readonly
     */
    static ChangeStateCommand = {
        play: 'play',
        pause: 'pause',
        stop: 'stop'
    }

    // Public methods

    get Name() {
        return this._name;
    }
    set Name(value) {
        this._name = value;
        this.#writeString(value, NeoDK.#AttributeId.BoxName)
    }
    /**
     * Method to select pattern to play
     * @public
     * @param {string} name name of the pattern to select
     */
    selectPattern(name) {
        this.#writeString(name, NeoDK.#AttributeId.CurrentPatternName);
    }

    /**
     * Method to set play state of the box
     * @public
     * @param {ChangeStateCommand} state one of the accepted play states from ChangeStateCommand
     */
    setPlayState(state) {
        this.#writeString(state, NeoDK.#AttributeId.PlayPauseStop);
    }

    /**
     * Method to set intensity of output
     * @public
     * @param {number} intensity the intensity between 0 and 100
     */
    setIntensity(intensity) {
        this.#sendAttrWriteRequest(this.#the_writer, NeoDK.#AttributeId.IntensityPercent, new Uint8Array([NeoDK.#Encoding.UnsignedInt1, intensity]));
    }

    refreshVoltages() {
        this.#sendAttrReadRequest(this.#the_writer, NeoDK.#AttributeId.Voltages);
    }

    /**
     * Method to get the port from browser
     * User will be prompted to select a port that box is connected to
     * @public
     * @param {Event} evt 
     */
    async selectPort(evt) {
        const filters = [
            { usbVendorId: 0x0403 },    // FTDI
            { usbVendorId: 0x067b },    // Prolific
            { usbVendorId: 0x10c4 },    // Silicon Labs
            { usbVendorId: 0x1a86 },    // WCH (CH340 chip)
            { usbVendorId: 0x16d0, usbProductId: 0x12ef },
			{ bluetoothServiceClassId: '00001101-0000-1000-8000-00805f9b34fb' }	// HC-05, HC-06 virtual ports
        ];
        var port = await navigator.serial.requestPort({ filters });
        port.onconnect = () => {
            this.logger.log('Connected', port.getInfo());
        }
        port.ondisconnect = () => {
            this.logger.log('Disconnected', port.getInfo());
            writer.releaseLock();
            writer = null;
        }
        return await this.#usePort(port);
    }

    static BoxPower = class NeoDKBoxPower {
        /**
         *
         */
        constructor() {
            this._batteryVoltage = 0;
            this._capacitorVoltage = 0;
            this._primaryCurrent = 0;
        }

        get BatteryVoltage() {
            return this._batteryVoltage;
        }
        set BatteryVoltage(value) {
            this._batteryVoltage = value;
        }

        get CapacitorVoltage() {
            return this._capacitorVoltage;
        }
        set CapacitorVoltage(value) {
            this._capacitorVoltage = value;
        }

        get PrimaryCurrent() {
            return this._primaryCurrent;
        }
        set PrimaryCurrent(value) {
            this._primaryCurrent = value;
        }

    }

    /**
     * Structure that represents play state of NeoDK
     */
    static State = class NeoDKState {
        constructor() {
            this._playState = NeoDK.#playStates[0];
            this._intensity = 0;
            this._currentPattern = '';
            this._availablePatterns = [];
            this.power = new NeoDK.BoxPower();
        }


        get PlayState() {
            return this._playState;
        }
        set PlayState(value) {
            this._playState = value;
        }

        get Intensity() {
            return this._intensity;
        }
        set Intensity(value) {
            this._intensity = value;
        }

        get CurrentPattern() {
            return this._currentPattern;
        }
        set CurrentPattern(value) {
            this._currentPattern = value;
        }

        get AvailablePatterns() {
            return this._availablePatterns;
        }
        set AvailablePatterns(value) {
            this._availablePatterns = value;
        }
    }

    // protocol constants

    /**
     * NeoDK Protocol: Structure Sizes
     * @private
     * @enum
     * @readonly
     */
    static #StructureSize = {
        FrameHeader: 8,
        MaxPayload: 512,
        PacketHeader: 6,
        AttributeAction: 6
    };

    /**
     * NeoDK Protocol: Frame Types
     * @private
     * @enum
     * @readonly
     */
    static #FrameType = {
        None: 0,
        Ack: 1,
        Sync: 3,
        Data: 4
    };

    /**
     * NeoDK Protocol: Network Service Types
     * @private
     * @enum
     * @readonly
     */
    static #NST = {
        Debug: 0,
        Datagram: 1
    };

    /**
     * NeoDK Protocol: Attribute Identifiers
     * @private
     * @enum
     * @readonly
     */
    static #AttributeId = {
        Voltages: 3,
        AllPatternNames: 5,
        CurrentPatternName: 6,
        IntensityPercent: 7,
        PlayPauseStop: 8,
        BoxName: 9
    };

    /**
     * NeoDK Protocol: Encodings
     * @private
     * @enum
     * @readonly
     */
    static #Encoding = {
        UnsignedInt1: 4,
        UTF8_1Len: 12,
        Bytes_1Len: 16,
        Array: 22,
        EndOfContainer: 24
    }

    /**
     * NeoDK Protocol: OP Codes (request types)
     * @private
     * @enum
     * @readonly
     */
    static #OPCode = {
        ReadRequest: 2,
        SubscribeRequest: 3,
        ReportData: 5,
        WriteRequest: 6,
        InvokeRequest: 8
    };



    /**
     * NeoDK Protocol: Play states
     * @private
     * @enum
     * @readonly
     */
    static #playStates = {
        "0": "undefined",
        "1": "stopped",
        "2": "paused",
        "3": "playing"
    };

    // private fields on NeoDK instance
    #rx_frame;
    #rx_nb = 0;
    #incoming_payload_size = 0;
    #the_writer = null;
    #tx_seq_nr = 0;
    #transaction_id = 1959;

    // private methods

    #writeString(value, attribute) {
        let enc_value = new TextEncoder().encode(value);
        const array = Array.from(enc_value); // Convert to regular array
        array.unshift(NeoDK.#Encoding.UTF8_1Len, enc_value.length); // Use unshift
        enc_value = Uint8Array.from(array);
        this.#sendAttrWriteRequest(this.#the_writer, attribute, enc_value);
    }

    #initFrame(payload_size, frame_type, service_type, seq) {
        const frame = new Uint8Array(NeoDK.#StructureSize.FrameHeader + payload_size);
        frame[0] = (service_type << 4) | (frame_type << 1);
        frame[1] = seq << 3;
        frame[2] = (payload_size >> 8) & 0xff;
        frame[3] = payload_size & 0xff;
        frame[4] = 0;
        return frame;
    }


    #crcFrame(frame) {
        frame[5] = this.#crc8_ccitt(0, frame, 5);
        let crc16 = this.#crc16_ccitt(0xffff, frame, 6);
        crc16 = this.#crc16_ccitt(crc16, frame.slice(NeoDK.#StructureSize.FrameHeader), frame.length - NeoDK.#StructureSize.FrameHeader);
        frame[6] = (crc16 >> 8);
        frame[7] = crc16 & 0xff;
        return frame;
    }


    #makeAckFrame(service_type, ack) {
        const frame = this.#initFrame(0, NeoDK.#FrameType.Ack, service_type, 0);
        frame[0] |= 1;
        frame[1] |= (ack & 0x7);
        return this.#crcFrame(frame);
    }


    #makeSyncFrame(service_type) {
        return this.#crcFrame(this.#initFrame(0, NeoDK.#FrameType.Sync, service_type, this.#tx_seq_nr++));
    }


    // not used... todo delete?
    #makeCommandFrame(cmnd_str) {
        const enc_cmnd = new TextEncoder().encode(cmnd_str);
        const frame = this.#initFrame(enc_cmnd.length, NeoDK.#FrameType.Data, NeoDK.#NST.Debug, this.#tx_seq_nr++);
        frame.set(enc_cmnd, FRAME_HEADER_SIZE);
        return this.#crcFrame(frame);
    }


    #makeRequestPacketFrame(trans_id, request_type, attribute_id, data) {
        let packet_size = NeoDK.#StructureSize.PacketHeader + NeoDK.#StructureSize.AttributeAction;
        if (data !== null) packet_size += data.length;
        const frame = this.#initFrame(packet_size, NeoDK.#FrameType.Data, NeoDK.#NST.Datagram, this.#tx_seq_nr++);
        let offset = NeoDK.#StructureSize.FrameHeader;
        // Initialise the packet header - to all zeroes, for now.
        for (let i = 0; i < NeoDK.#StructureSize.PacketHeader; i++) frame[offset++] = 0x00;
        frame[offset++] = trans_id & 0xff;
        frame[offset++] = (trans_id >> 8) & 0xff;
        frame[offset++] = request_type & 0xff;
        frame[offset++] = 0x00;
        frame[offset++] = attribute_id & 0xff;
        frame[offset++] = (attribute_id >> 8) & 0xff;
        if (data !== null) frame.set(data, offset);
        return this.#crcFrame(frame);
    }


    async #sendFrame(writer, frame) {
        // this.logger.log('this.#sendFrame, size=' + frame.length);
        await writer.write(frame);
    }


    #sendAttrReadRequest(writer, attribute_id) {
        this.#sendFrame(writer, this.#makeRequestPacketFrame(this.#transaction_id++, NeoDK.#OPCode.ReadRequest, attribute_id, null));
    }


    #sendAttrWriteRequest(writer, attribute_id, data) {
        this.#sendFrame(writer, this.#makeRequestPacketFrame(this.#transaction_id++, NeoDK.#OPCode.WriteRequest, attribute_id, data));
    }


    #sendAttrSubscribeRequest(writer, attribute_id) {
        this.#sendFrame(writer, this.#makeRequestPacketFrame(this.#transaction_id++, NeoDK.#OPCode.SubscribeRequest, attribute_id, null));
    }

    #handleIncomingDebugPacket(chunk) {
        if (typeof (this.logger.debug) == 'function') {
            this.logger.debug(new TextDecoder().decode(chunk));
        }
    }


    #unpackPatternNames(enc_names) {
        const decoder = new TextDecoder();
        let pos = 0;
        var patternNames = [];
        while (pos < enc_names.length && enc_names[pos] == NeoDK.#Encoding.UTF8_1Len) {
            pos += 1;
            const len = enc_names[pos++];
            let name = decoder.decode(enc_names.slice(pos, pos + len));
            patternNames.push(name);
            this.logger.log('  ' + name);
            pos += len;
        }
        this.state.AvailablePatterns = patternNames;
        return pos;
    }


    #handleReportedData(aa) {
        const attribute_id = aa[4] | (aa[5] << 8);
        let offset = NeoDK.#StructureSize.AttributeAction;
        const data_length = aa.length - offset;
        switch (attribute_id) {
            case NeoDK.#AttributeId.Voltages:
                if (data_length >= 8 && aa[offset] == NeoDK.#Encoding.Bytes_1Len) {
                    const Vbat_mV = aa[offset + 2] | ((aa[offset + 3]) << 8);
                    const Vcap_mV = aa[offset + 4] | ((aa[offset + 5]) << 8);
                    const Ipri_mA = aa[offset + 6] | ((aa[offset + 7]) << 8);
                    this.state.power.BatteryVoltage = Vbat_mV / 1000;
                    this.state.power.CapacitorVoltage = Vcap_mV / 1000;
                    this.state.power.PrimaryCurrent = Ipri_mA / 1000;
                    this.logger.log('Vbat=' + Vbat_mV + ' mV, Vcap=' + Vcap_mV + ' mV, Ipri=' + Ipri_mA + ' mA');
                }
                break;
            case NeoDK.#AttributeId.AllPatternNames:
                if (data_length >= 2 && aa[offset] == NeoDK.#Encoding.Array) {
                    this.logger.log('Available patterns:');
                    offset += this.#unpackPatternNames(aa.slice(offset + 1));
                }
                break;
            case NeoDK.#AttributeId.CurrentPatternName:
                if (aa[offset] == NeoDK.#Encoding.UTF8_1Len) {
                    const name = new TextDecoder().decode(aa.slice(offset + 2));
                    this.state.CurrentPattern = name;
                    this.logger.log('Current pattern is ' + name);
                }
                break;
            case NeoDK.#AttributeId.IntensityPercent:
                if (aa[offset] == NeoDK.#Encoding.UnsignedInt1) {
                    const intensity_perc = aa[offset + 1];
                    this.state.Intensity = intensity_perc;
                    this.logger.log('Intensity is ' + intensity_perc + '%');
                }
                break;
            case NeoDK.#AttributeId.PlayPauseStop:
                if (aa[offset] == NeoDK.#Encoding.UnsignedInt1) {
                    const play_state = aa[offset + 1];
                    if (play_state >= 4) play_state = 0;
                    this.state.PlayState = NeoDK.#playStates[play_state];
                    this.logger.log('NeoDK is ' + NeoDK.#playStates[play_state]);
                }
                break;
            case NeoDK.#AttributeId.BoxName:
                if (aa[offset] == NeoDK.#Encoding.UTF8_1Len) {
                    const name = new TextDecoder().decode(aa.slice(offset + 2));
                    this._name = name;
                    this.logger.log('Box name is ' + name);
                }
                break;
            default:
                this.logger.log('Unexpected attribute id: ' + attribute_id);
        }
    }


    #handleIncomingDatagram(datagram) {
        const offset = NeoDK.#StructureSize.PacketHeader;
        const opcode = datagram[offset + 2];
        if (opcode == NeoDK.#OPCode.ReportData) {
            this.#handleReportedData(datagram.slice(offset))
        } else {
            const tr_id = datagram[offset] | (datagram[offset + 1] << 8);
            this.logger.log('Transaction ID=' + tr_id + ', opcode=' + opcode);
        }
    }


    #assembleIncomingFrame(b) {
        this.#rx_frame[this.#rx_nb++] = b;
        // Collect bytes until we have a complete header.
        if (this.#rx_nb < NeoDK.#StructureSize.FrameHeader) return NeoDK.#FrameType.None;

        // Shift/append the input buffer until it contains a valid header.
        if (this.#rx_nb == NeoDK.#StructureSize.FrameHeader) {
            if (this.#rx_frame[5] != (this.#crc8_ccitt(0, this.#rx_frame, 5) & 0xff)) {
                this.#rx_nb -= 1;
                for (let i = 0; i < this.#rx_nb; i++) this.#rx_frame[i] = this.#rx_frame[i + 1];
                return NeoDK.#FrameType.None;
            }
            // Valid header received, start collecting the payload (if any).
            if ((this.#incoming_payload_size = (this.#rx_frame[2] << 8) | this.#rx_frame[3]) > NeoDK.#StructureSize.MaxPayload) {
                this.logger.log('Frame payload too big: ' + this.#incoming_payload_size + ' bytes');
                this.#incoming_payload_size = 0;
                this.#rx_nb = 0;
                return NeoDK.#FrameType.None;
            }
        }
        if (this.#rx_nb == NeoDK.#StructureSize.FrameHeader + this.#incoming_payload_size) {
            this.#rx_nb = 0;
            return (this.#rx_frame[0] >> 1) & 0x7;
        }
        return NeoDK.#FrameType.None;
    }


    #processIncomingData(value) {
        for (let i = 0; i < value.length; i++) {
            const frame_type = this.#assembleIncomingFrame(value[i]);
            if (frame_type == NeoDK.#FrameType.None) continue;

            if (frame_type == NeoDK.#FrameType.Ack) {
                const ack = this.#rx_frame[1] & 0x7;
                // this.logger.log('Got ACK ' + ack);
                continue;
            }

            const service_type = (this.#rx_frame[0] >> 4) & 0x3;
            if (frame_type == NeoDK.#FrameType.Data) {
                const seq = (this.#rx_frame[1] >> 3) & 0x7;
                this.#sendFrame(this.#the_writer, this.#makeAckFrame(service_type, seq));
            }
            // this.logger.log('Service type is ' + service_type + ', payload size is ' + this.#incoming_payload_size);
            if (this.#incoming_payload_size == 0) continue;

            const packet = this.#rx_frame.slice(NeoDK.#StructureSize.FrameHeader, NeoDK.#StructureSize.FrameHeader + this.#incoming_payload_size);
            if (service_type == NeoDK.#NST.Debug) {
                this.#handleIncomingDebugPacket(packet);
            } else if (service_type == NeoDK.#NST.Datagram) {
                this.#handleIncomingDatagram(packet);
            }
        }
    }


    async #readIncomingData(reader) {
        while (true) {
            const { value, done } = await reader.read();
            if (done) {
                reader.releaseLock();
                break;
            }
            // this.logger.log('Incoming data length is ' + value.length);
            this.#processIncomingData(value);
        }
    }


    async #usePort(port) {
        await port.open({ baudRate: 115200 }).then(() => {
            this.logger.log('Opened port ', port.getInfo());
            this.#the_writer = port.writable.getWriter();
            this.#sendFrame(this.#the_writer, this.#makeSyncFrame(NeoDK.#NST.Debug));

            this.#readIncomingData(port.readable.getReader());

            // We have one readable attribute and three we can subscribe to.
            this.#sendAttrReadRequest(this.#the_writer, NeoDK.#AttributeId.AllPatternNames);
            this.#sendAttrReadRequest(this.#the_writer, NeoDK.#AttributeId.Voltages);
            this.#sendAttrReadRequest(this.#the_writer, NeoDK.#AttributeId.BoxName);
            this.#sendAttrSubscribeRequest(this.#the_writer, NeoDK.#AttributeId.CurrentPatternName);
            this.#sendAttrSubscribeRequest(this.#the_writer, NeoDK.#AttributeId.IntensityPercent);
            this.#sendAttrSubscribeRequest(this.#the_writer, NeoDK.#AttributeId.PlayPauseStop);
        });
        return true;
    }


    #crc8_ccitt(crc, data, size) {
        for (let i = 0; i < size; i++) {
            crc ^= data[i];
            for (let k = 0; k < 8; k++) {
                crc = crc & 0x80 ? (crc << 1) ^ 0x07 : crc << 1;
            }
        }
        return crc;
    }


    #crc16_ccitt(crc, data, size) {
        for (let i = 0; i < size; i++) {
            crc ^= data[i] << 8;
            for (let k = 0; k < 8; k++) {
                crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
            }
        }
        return crc;
    }
}

export default NeoDK;

