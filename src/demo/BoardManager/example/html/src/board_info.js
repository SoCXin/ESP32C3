const boardInfoNames = ['title', 'tagline', 'appName', 'appVersion', 'link', 'compileTime', 'idfVersion', 'elfSHA256', 'hwInfo'];
class BoardInfo {
    constructor() {
        this.keyPrefix = 'data_';
        this.initialize();
    }

    initialize() {
        for (let i = 0; i < boardInfoNames.length; ++i) {
            this[this.keyPrefix + boardInfoNames[i]] = '';
        }
    }

    setFromString(jStr) {
        this.initialize();
        let cfg = JSON.parse(jStr);

        for (const [key, value] of Object.entries(cfg)) {
            if (this.hasOwnProperty(this.keyPrefix + key)) {
                this[this.keyPrefix + key] = value;
            }
        }
    }

    get(key) {
        if (this.hasOwnProperty(this.keyPrefix + key))
            return this[this.keyPrefix + key];

        return '';
    }
}
