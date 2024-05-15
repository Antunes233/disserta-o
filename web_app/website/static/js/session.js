// Connect to the MQTT broker
const mqtt = require("mqtt");
const client = new Paho.MQTT.Client('broker.emqx.io', 1883);
client.connect({
  onSuccess: () => {
    console.log("Connected to MQTT broker");
  },
  onFailure: (error) => {
    console.error("Failed to connect to MQTT broker:", error);
  },
});

// Add event listener to the button
const sendMessageBtn = document.getElementById("botão_sessão");
sendMessageBtn.addEventListener("click", sendMessage);

function sendMessage() {
  // Publish the MQTT message
  const message = new Paho.MQTT.Message("Hello, MQTT!");
  message.destinationName = "django/gait_values";
  client.send(message);

  // Open the subscriber page in a new window or tab
//   const subscriberUrl = "/subscriber"; // Replace with the actual URL of your subscriber page
//   window.open(subscriberUrl, "_blank");
}


