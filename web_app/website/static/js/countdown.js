// Set the countdown time in seconds
var countdownTime = 1 * 60;

// Function to start the countdown
function startTimer() {
    var timerDisplay = document.getElementById('timer');
    var data;

    data.innerHTML = "{{patient.id}}";

    var minutes, seconds;

    var countdownInterval = setInterval(function() {
        minutes = parseInt(countdownTime / 60, 10);
        seconds = parseInt(countdownTime % 60, 10);

        minutes = minutes < 10 ? '0' + minutes : minutes;
        seconds = seconds < 10 ? '0' + seconds : seconds;

        timerDisplay.textContent = minutes + ':' + seconds;

        if (--countdownTime < 0) {
            clearInterval(countdownInterval);
            timerDisplay.textContent = '00:00';

            // Redirect to another URL when the countdown finishes
            window.history.go(-1);
        }
    }, 1000);

// Start the countdown when the page loads
window.onload = startTimer;