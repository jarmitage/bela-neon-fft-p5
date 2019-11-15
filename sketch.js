// Visualising frequency domain data
// With naive/basic averaging
// And WebSocket msg debug printing

var freqDomainRealArr     = []
var freqDomainRealArrPrev = []
var freqDomainRealArrAvg  = []
var peaksArr = []
var printFreq  = 100
var printCount = 0
var FFTSize = 2048
var FFTSizeLog = Math.log(2048)

var guiSketch = new p5(function( p ) {

    Bela.data.target.addEventListener('buffer-ready', function(event) {
        if(event.detail == 1 && typeof Bela.data.buffers[1] != 'undefined') {

            for (i = 0; i < Bela.data.buffers[1].length; i++) {
                freqDomainRealArrPrev[i] = freqDomainRealArr[i]
                freqDomainRealArr[i]     = Bela.data.buffers[0][i]
                freqDomainRealArrAvg[i]  = (freqDomainRealArr[i]+freqDomainRealArrPrev[i])/2
            }

            if (++printCount >= printFreq) {
                // console.log('real', freqDomainRealArr)
                printCount = 0
            }

        }
    });

    p.setup = function() {
        p.createCanvas(p.windowWidth, p.windowHeight)
    }

    p.draw = function() {
        p.background(255)

        p.strokeWeight(2)
        p.stroke(255,0,0)
        p.beginShape()
        for (let i = 0; i < FFTSize; i++) {
            p.vertex(p.windowWidth*(Math.log(i)/FFTSizeLog), (p.windowHeight)+freqDomainRealArrAvg[i]*-10)
        }
        p.endShape();

    }

    function windowResized() {
      p.resizeCanvas(window.innerWidth, window.innerHeight);
    }
}, 'gui');
