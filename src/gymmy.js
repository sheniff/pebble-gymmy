var Service = function() {
  var APP_ID = 't2Fd3lSUvN5kdkrD4DKfGm49HLsmfM8ZC8801r0T',
      KEY = 'OOC98IEnI3QZwCOtMVlEWx3y68uN5aJlzkIWFHGQ',
      API = 'https://' + APP_ID + ':javascript-key=' + KEY + '@api.parse.com/1/classes',
      PUB_API = 'https://api.parse.com/1/classes',
      xhrRequest = function (url, type, callback) {
        var xhr = new XMLHttpRequest();
        xhr.onload = function () {
          callback(this.responseText);
        };

        xhr.open(type, url);
        console.log('dafuck??', type, url);
        xhr.send();
        console.log('it happened!');
      };
  
  return {
    Workout: function() {
      var _API = API + '/workout';
      
      return {
        get: function(id, callback) {
          var url = _API + (id ? '/' + id : '');
          xhrRequest(url, 'GET', callback);
        }
      };
    },
    Exercise: function() {
      var _API = API + '/exercise';
      
      return {
        get: function(id, callback) {
          var url = _API + (id ? '/' + id : '');
          xhrRequest(url, 'GET', callback);
        },
        getByWorkout: function(workoutId, callback) {
//           var url = _API + '?where={"workout":{"__type":"Pointer","className":"workout","objectId":"' + workoutId + '"}}';
          var url = _API; // ToDo: Modify this to work as the line above...
          xhrRequest(url, 'GET', callback);
        }
      };      
    }
  };
};

// Workout Class
var Workout = function(exercises) {
  this.exercises = exercises;    
  return this;
};

// TODO: Eventually include workout's name
Workout.prototype.sendInfo = function() {
  var dictionary = {
    KEY_MESSAGE_TYPE: 0,  // workout
    KEY_NUM_EXERCISES: this.exercises.length
  };

  console.log('sending workout info to pebble!', JSON.stringify(dictionary));

  Pebble.sendAppMessage(dictionary,
                        function(e) { console.log('Workout info sent to Pebble successfully!'); },
                        function(e) { console.log('Error sending workout info to Pebble!'); });
};


Workout.prototype.sendExercise = function(numExercise) {
  if(!this.exercises) {
    console.log('[sendWorkoutExercise:Error] There are not exercises yet!');
    return;
  }
  
  if(numExercise < this.exercises.length) {
//     var reps = [],
//         series = this.exercises[numExercise].series;
//     for (var i in series) {
//       series.push(series[i].getUInt8());
//     }
    var dictionary = {
      KEY_MESSAGE_TYPE: 1,  // exercise
      KEY_EXERCISE_NUMBER: numExercise,
      KEY_EXERCISE_NAME: this.exercises[numExercise].name,
      KEY_EXERCISE_SERIES: this.exercises[numExercise].series.length,
      KEY_EXERCISE_REPS: this.exercises[numExercise].series
    };

    console.log('[sendWorkoutExercise:Info] sending info to pebble!', JSON.stringify(dictionary));

    Pebble.sendAppMessage(dictionary,
                          function(e) { console.log('[sendWorkoutExercise:Info] Info sent to Pebble successfully!'); },
                          function(e) { console.log('[sendWorkoutExercise:Error] Error sending info to Pebble!'); });

  } else {
    console.log('[sendWorkoutExercise:Error] invalid num of exercise. Given: ' + numExercise + '. Max: ' + this.exercises.length);
  }
};

var currentWorkout = null;

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    Service().Exercise().getByWorkout('COg8bkZkfw', function(res){
      currentWorkout = new Workout(JSON.parse(res).results);
      currentWorkout.sendInfo();
    });
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    // ToDo: We can only receive messages requesting an exercise
    console.log('AppMessage received!', JSON.stringify(e));
    currentWorkout.sendExercise(e.payload.KEY_EXERCISE_NUMBER);
  }                     
);