export const sampleMissionJsonl = `
{"type":"measurement","t_meas_s":0.05,"t_sent_s":0.05,"sensor_id":1,"sensor_type":"Radar","measurement_type":"RangeBearing2D","z_dim":2,"z":[1200.0,0.35],"R":[25.0,0.0,0.0,0.01],"confidence":0.92,"trace_id":1,"causal_parent_id":0}

{"type":"track_event","event":"track_updated","t_s":0.10,"track_id":10,"status":"Confirmed","x":[1200.0,420.0,50.0,80.0,12.0,0.0],"P":[1,0,0,0,0,0],"age_ticks":4,"hits":4,"misses":0,"score":4.2,"confidence":0.94,"trace_id":2,"causal_parent_id":1}
{"type":"track_stability_event","event":"track_confirmed","t_s":0.10,"track_id":10,"status":"Confirmed","age_ticks":4,"hits":4,"misses":0,"score":4.2,"confidence":0.94,"reason":"confirm_hits_reached","trace_id":3,"causal_parent_id":2}
{"type":"interceptor_state","t_s":0.10,"interceptor_id":1,"engaged":0,"track_id":0,"position":[0.0,0.0,0.0],"velocity":[0.0,0.0,0.0],"engagement_time_s":0.0,"trace_id":4,"causal_parent_id":0}
{"type":"bt_decision","t_s":0.10,"tick":2,"mode":"engage","track_id":10,"decision_type":"engage","reason":"engage_start","engagement_commands":1,"active_engagements":1,"idle_interceptors":2,"reconciliation_removals":0,"active_engagements_after_reconcile":0,"selected_track_id":10,"selected_interceptor_id":1,"selected_engagement_score":1.82,"selected_estimated_intercept_time_s":8.5,"assignment_reason":"intercept_aware_idle_assignment","events":[{"event":"engage_start","track_id":10,"interceptor_id":1,"reason":"intercept_aware_idle_assignment"}],"nodes":[{"node":"engage.target_ready","status":"success","detail":"confirmed_track_ready"}],"trace_id":5,"causal_parent_id":2}
{"type":"assignment_event","t_s":0.10,"interceptor_id":1,"track_id":10,"decision_type":"engage_assigned","reason":"intercept_aware_idle_assignment","trace_id":6,"causal_parent_id":2}
{"type":"fault_event","t_s":0.15,"fault":"radar_jitter","trace_id":7,"causal_parent_id":0}
{"type":"interceptor_state","t_s":0.20,"interceptor_id":1,"engaged":1,"track_id":10,"position":[80.0,28.0,3.0],"velocity":[140.0,50.0,5.0],"engagement_time_s":0.1,"trace_id":8,"causal_parent_id":6}
{"type":"intercept_event","t_s":0.35,"track_id":10,"outcome":"intercept_success","reason":"distance_threshold_met","trace_id":9,"causal_parent_id":6}
`.trim();
