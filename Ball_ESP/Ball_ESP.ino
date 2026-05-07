#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// --- CONFIGURABLE THRESHOLDS ---
float WINDUP_THRESHOLD = 15.0;  // Force needed to start a shot
float RELEASE_THRESHOLD = 5.0;  // Force to trigger freefall
float IMPACT_THRESHOLD = 15.0;  // Force of hitting the target/pillow
float TWIST_THRESHOLD = 7.0;    // Gyro spin speed needed to unlock (rad/s)

// --- GLOBAL VARIABLES ---
bool isUnlocked = false;       
bool twistedLeft = false;     
bool twistedRight = false;     
unsigned long twistTimer = 0;  

bool isThrowing = false;
bool inCooldown = false;
float currentVelocity = 0.0;
float finalLaunchSpeed = 0.0;
unsigned long lastTime = 0;

// --- THE STOPWATCHES ---
unsigned long windupStartTime = 0;  
unsigned long releaseTime = 0;      
unsigned long cooldownStartTime = 0; 

void setup() {
  Serial.begin(115200);
  mpu.begin();
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G); 
  mpu.setGyroRange(MPU6050_RANGE_1000_DEG); 
  
  Serial.println("=================================");
  Serial.println("    SMART BALL SYSTEM BOOTED     ");
  Serial.println("=================================");
  Serial.println("Status: LOCKED.");
  Serial.println("Action: Twist left then right to unlock!");
}

void loop() {
  calculateShotSpeed();
  delay(10); 
}

void calculateShotSpeed() {
  

  if (inCooldown) {
    if (millis() - cooldownStartTime > 3000) {
      inCooldown = false; 
      

      twistedLeft = false;
      twistedRight = false; 
      
      Serial.println("\nCooldown finished. Ball is LOCKED.");
      Serial.println("Twist left/right to unlock for the next shot!");
    }
    return; 
  }


  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  // GESTURE UNLOCK 
  if (!isUnlocked) {
    float spinx = g.gyro.x; 

    if (spinx > TWIST_THRESHOLD) {
      twistedLeft = true;
      twistTimer = millis(); 
    }
    
 
    if (spinx < -TWIST_THRESHOLD) {
      twistedRight = true;
      twistTimer = millis(); 
    }

 
    if (twistedLeft && twistedRight) {
      isUnlocked = true;
      Serial.println("\n*** BALL UNLOCKED! ***");
      Serial.println("Ready for shot...");
      
      twistedLeft = false; 
      twistedRight = false;
      delay(500); 
    }

    if ((twistedLeft || twistedRight) && (millis() - twistTimer > 2000)) {
      twistedLeft = false;
      twistedRight = false;
      Serial.println("Unlock failed: Too slow. Try again.");
    }
    
    return; 
  }

  // TIME TRACKING 
  unsigned long currentTime = millis();
  float dt = (lastTime > 0) ? (currentTime - lastTime) / 1000.0 : 0;
  lastTime = currentTime;

  // total force
  float aMag = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z));


  if (!isThrowing && finalLaunchSpeed == 0.0 && aMag > WINDUP_THRESHOLD) {
    isThrowing = true;
    currentVelocity = 0.0; 
    finalLaunchSpeed = 0.0;
    windupStartTime = millis(); 
    Serial.println("Shot started... Building speed.");
  }


  if (isThrowing) {
    
    // detecting fake throws (WIP)
    if (millis() - windupStartTime > 1500) {
      isThrowing = false;
      Serial.println("Error: Fake throw detected. Resetting to UNLOCKED state.");
      inCooldown = true;
      cooldownStartTime = millis() - 2000; 
      return; 
    }

    float forwardAccel = aMag - 9.81; 
    if (forwardAccel > 0) {
       currentVelocity += (forwardAccel * dt);
    }

    if (aMag < RELEASE_THRESHOLD) { 
      isThrowing = false;
      finalLaunchSpeed = currentVelocity; 
      releaseTime = millis(); 
      
      Serial.println("--- BALL RELEASED! ---");
      Serial.print("Initial Launch Speed: ");
      Serial.print(finalLaunchSpeed);
      Serial.println(" m/s");
    }
  }

  if (!isThrowing && finalLaunchSpeed > 0 && aMag > IMPACT_THRESHOLD) {
    float timeOfFlight = (millis() - releaseTime) / 1000.0; 
    
    float releaseHeight = 2.0; 
    float deltaY = 3.05 - releaseHeight; 
    
    // math to calculate angle and shot distance from user
    float mathInsideArcsin = (deltaY + (4.905 * sq(timeOfFlight))) / (finalLaunchSpeed * timeOfFlight);
    mathInsideArcsin = constrain(mathInsideArcsin, -1.0, 1.0); 
    
    float launchAngle = asin(mathInsideArcsin) * (180.0 / PI);
    float shotDistance = finalLaunchSpeed * cos(asin(mathInsideArcsin)) * timeOfFlight;

    //data for testing
    Serial.println("==========================");
    Serial.println("      IMPACT DETECTED!    ");
    Serial.print("Airtime: "); Serial.print(timeOfFlight); Serial.println(" seconds");
    Serial.print("Launch Angle: "); Serial.print(launchAngle); Serial.println(" degrees");
    Serial.print("Shot Distance: "); Serial.print(shotDistance); Serial.println(" meters");
    Serial.println("==========================");
    

    finalLaunchSpeed = 0.0; 
    isUnlocked = false; 
    
    inCooldown = true;
    cooldownStartTime = millis();
    Serial.println("Cooling down (Ignoring bounces)...");
  }
}
