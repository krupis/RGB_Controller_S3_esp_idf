

var active_animation = 0;


//ADJUST COLOR FOR FADE IN OUT
const colorInput = document.getElementById('color_fade_in_out');
const speedSlider = document.getElementById('speed_fade_in_out');
const lights = document.querySelectorAll('.running_lights ul li');
const fading_in_out_circle = document.querySelectorAll('.circle_fade_in_out'); // fading in out

const circleContainers = document.querySelectorAll('.circle-container');

//CHANGE COLOR
      circleContainers.forEach(container => {
      changeBoxShadowColorInAnimation(container, '#ffff00');
  });
  

  colorInput.addEventListener('input', () => {
    const newColor = colorInput.value;
    for (let i = 0; i < lights.length; i++) {
      if(active_animation == 2){
        fading_in_out_circle[i].style.backgroundColor = `${newColor}`;
      }
    }
    circleContainers.forEach(container => {
      changeBoxShadowColorInAnimation(container, newColor);
    });
  });


//CHANGE SPEED
function updateAnimationSpeed() {
  console.log("adjusting speed ");
  const animationSpeed = 6 - speedSlider.value; // Inverse mapping from slider value to animation speed
  fading_in_out_circle.forEach(fading_in_out_circle => {
    fading_in_out_circle.style.animation = `grow ${animationSpeed}s ease-in-out infinite`;
  });
}

speedSlider.addEventListener('input', updateAnimationSpeed);
















  const colorInput_fade_in = document.getElementById('color_fade_in');
  const speedInput_fade_in = document.getElementById('speed_fade_in');
  const lights_fading_in = document.querySelectorAll('.fading_in_lights ul li');
  const fading_in_circle = document.querySelectorAll('.circle_fade_in'); // fading in 
  

//CHANGE COLOR
  colorInput_fade_in.addEventListener('input', () => {

    const newColor = colorInput_fade_in.value;
    for (let i = 0; i < lights_fading_in.length; i++) {
      console.log("chaning fading in color");
      if(active_animation == 1){
        fading_in_circle[i].style.backgroundColor = `${newColor}`;
      }
    }
    circleContainers.forEach(container => {
      changeBoxShadowColorInAnimation(container, newColor);
    });
    fading_in_circle.forEach(fading_in_circle => {
      fading_in_circle.classList.remove('circle_fade_in'); // Remove animation class
      void fading_in_circle.offsetWidth;
      fading_in_circle.classList.add('circle_fade_in'); // Add animation class back
    });
  });


  //CHANGE SPEED

  function updateAnimationSpeed2() {
    console.log("adjusting speed = ",speedInput_fade_in.value);
    
    fading_in_circle.forEach(fading_in_circle => {
      const animationSpeed = 6 - speedInput_fade_in.value; // Inverse mapping from slider value to animation speed
      fading_in_circle.style.animationDuration = animationSpeed.toString() + "s";
      fading_in_circle.classList.remove('circle_fade_in'); // Remove animation class
      void fading_in_circle.offsetWidth;
      fading_in_circle.classList.add('circle_fade_in'); // Add animation class back
    });
  }
  
  speedSlider.addEventListener('input', updateAnimationSpeed);
  speedInput_fade_in.addEventListener('input', updateAnimationSpeed2);





// Function to change box shadow color within the animation
function changeBoxShadowColorInAnimation(element, color) {
  element.style.setProperty('--box-shadow-color', color);
}









function user_mode_popup_configure(sel) {
  hide_div("test_div");

}

function user_mode_popup_cancel(sel) {
  hide_div("test_div");
}


function hide_div(div) {
  console.log("hiding div");
  var document_div = document.getElementById(div);
  document_div.style.display = "none";
}

function unhide_div(div) {
  console.log("unhiding div");
  var document_div = document.getElementById(div);
  document_div.style.display = "block";  

}

function Handle_animation_change(value){
  console.log("value = ",value);
  if(value == 1){
    hide_div("fading_in_out");
    unhide_div("fading_in");
  }
  else if(value == 2){
    hide_div("fading_in");
    unhide_div("fading_in_out");
  }
  active_animation = value;
}