var serverHost = window.location.hostname;

var server = null;
if(window.location.protocol === 'http:')
	server = "http://" + serverHost + ":8088/janus";
else
	server = "https://" + serverHost + ":8089/janus";

var janus = null;
var streaming = null;
var opaqueId = "streamingtest-"+Janus.randomString(12);


$(document).ready(function() {
	// Initialize the library (all console debuggers enabled)
	Janus.init({debug: "all", callback: function() {
	// Use a button to start the demo
	$(this).attr('disabled', true).unbind('click');
	// Make sure the browser supports WebRTC
	if(!Janus.isWebrtcSupported()) {
		bootbox.alert("No WebRTC support... ");
		return;
	}
	// Create session
	janus = new Janus(
		{
			server: server,
			success: function() {
				// Attach to streaming plugin
				janus.attach(
					{
						plugin: "janus.plugin.gstreamer",
						opaqueId: opaqueId,
						success: function(pluginHandle) {
							streaming = pluginHandle;
							Janus.log("Plugin attached! (" + streaming.getPlugin() + ", id=" + streaming.getId() + ")");

							$('#watch').attr('disabled', true).unbind('click');
							$('#watch').removeAttr('disabled').click(startStream);
						},
						error: function(error) {
							Janus.error("  -- Error attaching plugin... ", error);
							bootbox.alert("Error attaching plugin... " + error);
						},
						onmessage: function(msg, jsep) {
							Janus.debug(" ::: Got a message :::");
							Janus.debug(msg);
							var result = msg["result"];
							if(result !== null && result !== undefined) {
								if(result["status"] !== undefined && result["status"] !== null) {
									var status = result["status"];
									if(status === 'stopped')
										stopStream();
								}
							} else if(msg["error"] !== undefined && msg["error"] !== null) {
								bootbox.alert(msg["error"]);
								stopStream();
								return;
							}
							if(jsep !== undefined && jsep !== null) {
								Janus.debug("Handling SDP as well...");
								Janus.debug(jsep);
								// Offer from the plugin, let's answer
								streaming.createAnswer(
									{
										jsep: jsep,
										media: { audioSend: false, videoSend: false },	// We want recvonly audio/video

										success: function(jsep) {
											Janus.debug("Got SDP!");
											Janus.debug(jsep);
											var body = { "request": "start" };
											streaming.send({"message": body, "jsep": jsep});
											$('#watch').html("Stop").removeAttr('disabled').click(stopStream);
										},
										error: function(error) {
											Janus.error("WebRTC error:", error);
											bootbox.alert("WebRTC error... " + JSON.stringify(error));
										}
									});
							}
						},
						onremotestream: function(stream) {
							Janus.debug(" ::: Got a remote stream :::");
							Janus.debug(stream);
							Janus.attachMediaStream($('#remotevideo').get(0), stream);
						},
						oncleanup: function() {
							Janus.log(" ::: Got a cleanup notification :::");
						}
					});
			},
			error: function(error) {
				Janus.error(error);
				bootbox.alert(error);
			},
			destroyed: function() {
				window.location.reload();
			}
		});
	}});
});

function startStream() {
	var mrl = $('#source-edit').val();
	if(mrl === undefined || mrl === null || !mrl) {
		bootbox.alert("Enter source MRL");
		return;
	}
	$('#watch').attr('disabled', true).unbind('click');
	var body = { "request": "watch", mrl: mrl };
	streaming.send({"message": body});
}

function stopStream() {
	$('#watch').attr('disabled', true).unbind('click');
	var body = { "request": "stop" };
	streaming.send({"message": body});
	streaming.hangup();
	$('#watch').html("Watch").removeAttr('disabled').click(startStream);
}
