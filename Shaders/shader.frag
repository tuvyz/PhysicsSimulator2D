uniform sampler2D image;
uniform float opacity;
varying vec4 vTexCoord;


void main(void) {
  vec4 color = texture2D(image, vTexCoord.xy);

  color.a = color.a * opacity;

  gl_FragColor = color;
}
